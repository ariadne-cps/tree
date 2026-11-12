# DerivationTree design and initial implementation

Status: the initial generic structure is implemented in
`include/tree/derivation_tree.hpp`. Pruning/regrowth and recombination remain
future implementation work. The domain-specific Evolution wrapper is not implemented.

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
and memory address. IDs are never reused; future pruning must preserve this
rule. Foreign-tree IDs are rejected. The owner controls lifetimes; parent and child links are
non-owning identifiers. Storage growth must not invalidate IDs.

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

Nodes are stored directly in a `std::deque`, with flat ownership. Inserting
nodes does not relocate existing payloads, and destroying a deep tree does not
recursively follow children. Each node stores its parent index and a
geometrically growing vector of child IDs. This avoids one separate node
allocation per insertion and avoids quadratic copying of a wide child list.
Allocating a deque block or growing a child vector can still allocate memory.

`DerivationTreeNodeId` is an opaque owner token and node index. It is comparable
and streamable; default construction produces an invalid ID. Tree ownership
tokens are allocated by a shared counter across template instantiations and are
never recycled, including after destruction. IDs are process-local handles,
not serialization keys. Owner-counter exhaustion throws `std::overflow_error`.

Lookup, containment, parent and borrowed children access take O(1). Node
insertion takes amortized O(1), excluding payload construction; node storage
does not copy/move existing payloads. Ancestor traversal takes O(depth) and
returns immediate parent first. Descendants are returned in insertion-ordered
depth-first preorder in O(visited nodes), using an iterative traversal. Both
queries exclude the node itself and allocate their result vectors.

Payload references survive insertion and tree moves. `children(id)` returns a
`std::span<const NodeId>`: that parent's child-vector growth may invalidate its
views, including when a subsequent payload constructor throws. Reacquire views
after attempted insertion under their parent. References/views expire when
their nodes are destroyed. No removal operation exists in this version.

Copying a tree is disabled so that duplicating an owner cannot alias its IDs.
Moving transfers the nodes and their IDs without moving individual payloads;
the source becomes a fresh empty tree with an independent ownership token.
Move assignment destroys the destination's previous nodes and invalidates
their references and IDs. Self-move assignment has no effect. Moves may allocate
the empty replacement owner/storage and are not declared `noexcept`.

Concurrent mutation is unsupported; shared access involving a writer requires
external synchronization. Read-only access can be shared. Payload constructors
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

The current implementation supports growth. **Pruning is the missing generic
operation needed to complete this structural model.** Stable IDs alone do not
provide pruning or backtracking.

### Pruning contract to settle before implementation

- Define exactly which nodes are removed and which checkpoint is retained for
  regrowth. Pruning a child subtree and pruning all descendants of a node are
  different boundaries; an API must state which it performs.
- Remove pruned nodes from active genealogy and reject their IDs on lookup.
- Never reuse IDs issued to removed nodes when the tree regrows.
- Preserve identities and payload references for retained nodes. End the borrowed
  reference/view lifetime of removed nodes and document affected children views.
- Permit adding new children at the retained boundary after pruning.
- Define storage reclamation without shifting retained nodes: erasing a middle
  range from the current deque is not a suitable shortcut for these guarantees.

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
| Prune and regrow | Not implemented | Define retained boundary, stale-ID rejection and reference/storage guarantees |
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

Planned pruning tests must cover removal of exactly the requested branch,
retained-node identity/reference stability, stale-handle rejection, regrowth
with fresh IDs, and pruning at root/leaf boundaries. They are future tests;
there is no pruning API in the current header.

## Verification

`tests/test_derivation_tree.cpp` exercises mutable ordered payloads and
node-local completion, genealogy order, 4096 children with immovable payloads,
a 20000-edge chain, invalid/foreign/expired IDs, tree moves with move-only
payloads, and throwing object/metadata constructors. The installed-library
C++ tutorial demonstrates continued parent progression after child creation.

The initial container stores completion as application metadata; a hybrid
scheduler is not part of these tests. Later capabilities must retain the
distinction between progression inside an object and derivation of a new object.
