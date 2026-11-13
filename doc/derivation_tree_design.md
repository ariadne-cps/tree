# DerivationTree design and initial implementation

Status: the initial generic structure is implemented in
`include/tree/derivation_tree.hpp`, including pruning and regrowth. Recombination
and the domain-specific Evolution wrapper are not implemented.

## Starting requirements and boundary

The initial consumer needs a genealogy of trajectories. Each trajectory is one
node containing an ordered list of sets. Parent-child relations identify which
trajectories originate from another trajectory. Every trajectory node also
records whether its own evolution is complete or still needs work.

Generic Tree stores objects and metadata without interpreting trajectories,
sets, numerical configurations or completion criteria. The hybrid contract
belongs in `ariadne-cps/evolution/doc/evolution_tree_design.md`.

This is a new abstraction, separate from the existing `BinaryTreeNode`. Its
Boolean binary-prefix interpretation and public contract remain unchanged.

## Representation

The owning `DerivationTree<Object, Metadata>` has exactly two template parameters.
The application chooses the object; for Evolution it can be a trajectory
containing an ordered sequence. Both types may be non-default-constructible or
move-only. Forwarded construction also permits immovable payloads.

```text
Node
    NodeId
    Object
    Metadata
    optional parent NodeId
    ordered list of child NodeIds
```

The root has no parent. Every other node has exactly one parent. A child is a
derived object, rather than simply the next state of the parent's object.
Application metadata can describe why the child was born, the position within
the parent object that produced it, and whether processing of the node is
complete. These fields need no hybrid meaning in generic Tree.

Node identity is stable, allocated independently of path, depth, sibling index
and memory address. IDs are never reused, including after pruning.
Foreign-tree IDs are rejected. The owner controls lifetimes; parent and child
links are non-owning identifiers. Storage growth must not invalidate IDs.

## Object progression and genealogy

Updating an existing object and creating a child are separate operations.
Appending a set to a trajectory updates the existing node. A new trajectory is
a new child. Generic Tree does not copy the parent's history into the child.

There are two distinct orders: the order of children and the order within the
stored object. Neither is a node identifier. Several children may originate at
different positions within one parent's object; application metadata identifies
those positions.

Creating a child does not consume or freeze the parent. A node may have children
and still need processing. Structural leaves therefore do not automatically
represent the active computational frontier.

Object and metadata mutation are sequential, through `object(id)` and
`metadata(id)` references. Const trees expose const references. Callers cannot
mutate parent or child links directly. Child creation publishes the object,
metadata and links together, or leaves the logical tree unchanged on failure.

## Storage, lifetime and cost

Nodes occupy stable `std::deque<Slot>` storage. Each slot has an optional node,
a generation number and a link used while the slot is free. Growing the deque
or recycling a vacant slot never moves a retained payload. Each live node has a
parent index and a geometrically growing vector of ordered child IDs.

`DerivationTreeNodeId` is an opaque ownership token, slot index and generation.
It is comparable and streamable; default construction produces an invalid ID.
Tree ownership tokens use a shared counter across template instantiations.
Nodes do not require a global counter: a slot gains a generation when its node
is pruned. Reusing that slot therefore produces a distinct identifier. An
exhausted generation retires the slot permanently rather than wrapping. Owner-
token exhaustion throws `std::overflow_error`. IDs and their diagnostic
`owner:slot:generation` representation are process-local, not serialization keys.

Freed slots form an intrusive free list and are reused before storage grows.
Payloads and their child-vector resources are destroyed immediately when pruned;
slot storage remains reserved up to its high-water mark for reuse. The retained
boundary also keeps its child-vector capacity. Except for permanently retired
slots at generation exhaustion, slot storage follows peak simultaneous demand,
not the accumulated number of growth/pruning cycles. Reclamation does not shift
retained nodes and does not require a heap allocation per node.

Lookup, containment, parent and borrowed children access take O(1). Node
insertion takes amortized O(1), excluding payload construction, and does not
copy/move existing payloads. Ancestor traversal takes O(depth) and returns the
immediate parent first. Descendants use insertion-ordered depth-first preorder
in O(visited nodes). Both queries exclude the input node and allocate result
vectors.

Pruning takes O(removed nodes) time plus payload destruction, O(1) auxiliary
storage and no allocation after identifier validation. It follows parent and
last-child links in iterative postorder, so a deep branch uses no recursive
call stack. `size()` reports live nodes, excluding free or retired slots.

Payload references survive growth, tree moves and pruning elsewhere. References
to removed payloads expire immediately even if their slots are later reused.
`children(id)` returns a borrowed `std::span<const NodeId>`: reacquire the view
after attempted growth under that parent (even failed construction) or pruning
its descendants. Views of removed nodes expire; views of unrelated retained
nodes are unaffected.

Copying a tree is disabled so that duplicating an owner cannot alias its IDs.
Moving transfers the nodes and their IDs without moving individual payloads;
the source becomes a fresh empty tree with an independent ownership token.
Move assignment destroys the destination's previous nodes and invalidates
their references and IDs. Self-move assignment has no effect. Moves may allocate
the empty replacement owner/storage and are not declared `noexcept`.

Concurrent mutation is unsupported; shared access involving a writer requires
external synchronization. Read-only access can be shared. Payload constructors and destructors
must not reenter the same tree. Owner-token allocation is synchronized, which
does not make operations on a tree synchronized.

## Completion and scheduling

Evolution stores node-local completion in its metadata. This means that the
trajectory itself has finished generating sets and child trajectories in the
current forward computation; it says nothing about descendant completion.
Tree does not decide when an application may mark an object complete.

A consumer may finish processing a parent before processing its children. At
that boundary, it can inspect the complete collection of children generated by
the parent. This policy is external to the generic container: Tree supplies
object/metadata access and genealogy queries, not a scheduler.

The completion marker describes finished generation, not immutable topology for
all future editing. If recombination later changes the active child collection,
the original generated collection/provenance must remain distinguishable.

## Public API

The public header is available directly or through `ariadne-tree.hpp`:

```text
create_root(object, metadata) -> NodeId
add_child(parent, object, metadata) -> NodeId
prune(NodeId) -> removed descendant count
empty(), size(), contains(NodeId)
root() -> optional NodeId
object(NodeId), metadata(NodeId)
parent(NodeId) -> optional NodeId
children(NodeId) -> ordered child IDs
ancestors(NodeId), descendants(NodeId)
```

Adding a child is permitted when the parent already has children. Application
code enforces its completion and append rules. Invalid, default or foreign IDs
throw `std::out_of_range` on access; `contains` returns false. An empty tree has
no root. Creating a second root throws `std::logic_error`. Constructors forward
their arguments directly into the stored object and metadata.

Insertion failure leaves node count, existing payloads and genealogy unchanged,
although a children view can be invalidated as described above. The tree does
not roll back moves or arbitrary side effects made by a user-supplied payload
constructor on its input arguments.

## Structural model: growth, pruning and regrowth

Only two forms of structural change are allowed:

1. Grow the tree by adding new child nodes to an existing node.
2. Prune an unwanted branch or future continuation, then allow fresh growth from
   the retained pruning boundary.

Inserting a node between existing nodes and moving/reparenting an existing
subtree are not options, including for backtracking. Existing nodes are not
relocated in the genealogy. Moving the owning C++ container transfers ownership
without changing that genealogy and is a separate operation.

The implementation supports growth and `prune(id)`, which removes all strict
descendants and retains id itself as the boundary for regrowth. It does not
remove a selected node from its parent's child list. Root pruning keeps the
root; pruning a leaf is a no-op. There is no intermediate insertion or
reparenting API.

### Pruning contract

- The boundary keeps its object, metadata, ID, parent and sibling position.
- All strict descendants are destroyed, disappear from traversal and fail
  `contains` and subsequent access, including another `prune` call.
- Pruned identifiers never become valid again during slot reuse.
- The return value is the removed node count; live `size()` decreases by that
  count and the boundary remains a leaf ready for `add_child`.
- Other branches, their order, identifiers and payload references are unchanged.
- Invalid, pruned or foreign input IDs throw `std::out_of_range` before mutation.
- The operation allocates no temporary traversal storage and cannot fail after
  validation under the required nonthrowing, non-reentrant destructor contract.
- Metadata is application-owned: pruning does not automatically reset completion
  flags or truncate the boundary's trajectory list.

A payload may itself contain a trajectory history. Truncating that history and
invalidating children derived from removed entries are consumer responsibilities
that must be coordinated with structural pruning. Tree does not infer that
relationship from arbitrary Object or Metadata types.

### Current coverage and remaining work

| Capability | Current support | Next decision |
| --- | --- | --- |
| Store and continue a trajectory after producing children | Mutable object/metadata access | Already supported |
| Mark local completion or logical retirement | Consumer metadata can carry the status | Active-versus-historical selection belongs to Evolution |
| Inspect genealogy | Parent, ordered children and materialized ancestor/descendant lists | Allocation-free visitors/ranges are an optional later optimization |
| Prune and regrow | `prune(id)` retains the boundary; `add_child` reuses slots with fresh generations | Implemented; payload truncation and completion reset remain consumer policy |
| Recombine selected sibling trajectories | Source groups can be inspected | Record provenance and create replacements; prune or retire superseded branches without moving them |
| Clone or persist a tree | Copy disabled; no serialization | Optional independent ownership/identity mappings if later needed |

For recombination, replacements are newly created trajectories, rather than
existing nodes grafted elsewhere. Preserve the contribution of removed sources
in metadata or a separate provenance record before pruning them. The ordinary
one-parent genealogy does not itself encode a mathematical multi-source union,
and historical source IDs must not be assumed to remain dereferenceable after
pruning.

Completion criteria, hybrid-time compatibility, overlap/coverage tests and
scheduling priorities remain domain policies. They require neither a third
template parameter nor a built-in hybrid operation enum. Persistence and
concurrent mutation are not prerequisites for the initial workflow.

Pruning tests cover exact descendant removal, unaffected branches and ordering,
root/leaf boundaries, payload reclamation, repeated regrowth without reviving
stale handles, throwing construction in recycled slots, and tree moves with
vacant slots. The 20000-edge chain is also pruned iteratively.

## Verification

### Python binding contract

The Python module exposes `DerivationTree` as a binding of the generic core with
Python object and metadata references, not a specialization to tutorial types.
Its root/child creation, access, genealogy and pruning operations correspond to
the C++ API. Optional IDs become None and collections become snapshot lists.
Node IDs compare by owner, slot and generation; they are opaque and unhashable.
Invalid/pruned/foreign lookup raises IndexError; duplicate-root creation raises
RuntimeError.

`object(id)` and `metadata(id)` return the stored Python objects themselves.
References obtained before pruning keep those Python objects alive under normal
Python ownership, while the old node ID remains invalid. No implicit copying
or conversion of user-defined payload classes is performed.

The binding reports all live payload references to Python's cyclic garbage
collector and guards instances allocated by __new__ before C++ initialization.
Clearing cycles replaces the core before dropping its references. During Python
pruning, temporary strong references defer payload finalizers until C++ pruning
is complete, allowing finalizers to observe a consistent tree. This adapter
uses O(removed nodes) temporary references in addition to the core's O(1)
auxiliary pruning storage. GC traversal also uses a snapshot of live node IDs.
Binding calls keep the GIL; the generic core's concurrency contract is unchanged.

C++ and Python tutorials must evolve together, including any bindings needed
to mirror an operation. The examples contain direct operations only. Binding
regressions and a check of the tutorial's Python scenario belong in
`python/tests/test_derivation_tree.py`, not in either tutorial.

`tests/test_derivation_tree.cpp` exercises mutable ordered payloads and
node-local completion, genealogy order, 4096 children with immovable payloads,
a 20000-edge chain, invalid/foreign/expired IDs, tree moves with move-only
payloads, throwing object/metadata constructors, and pruning/regrowth with
slot reuse. The installed-library C++ tutorial demonstrates parent progression,
pruning a retained checkpoint and regrowing with a distinct child identifier.

The initial container stores completion as application metadata; a hybrid
scheduler is not part of these tests. Later capabilities must retain the
distinction between progression inside an object and derivation of a new object.
