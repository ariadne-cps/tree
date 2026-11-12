# DerivationTree design and initial implementation

Status: the initial generic structure is implemented in
`include/tree/derivation_tree.hpp`. Recombination and historical editing remain
future design work. The domain-specific Evolution wrapper is not implemented.

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
and memory address. IDs are not reused after removal or invalidation. Foreign-
tree IDs are rejected. The owner controls lifetimes; parent and child links are
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

## Later capabilities

The earlier checkpoint-per-node and mandatory multi-input-graph proposals are
superseded as the starting abstraction. Recombination, historical insertion,
backtracking, numerical alternatives, persistence and concurrent mutation are
subsequent design work.

Future recombination may need additional provenance links beyond a single
genealogical parent. The current tree alone does not describe a multi-parent
merge. Absorbed objects cannot be discarded before their contribution and
replacement are recorded. Those requirements do not force a multi-input
transaction API into the initial generic implementation.

Stable node IDs allow later editing without renumbering unaffected nodes. They
do not prove that objects computed before an edit remain valid. An application
referencing a position within an object must define how that reference behaves
if entries are inserted or invalidated.

## Verification

`tests/test_derivation_tree.cpp` exercises mutable ordered payloads and
node-local completion, genealogy order, 4096 children with immovable payloads,
a 20000-edge chain, invalid/foreign/expired IDs, tree moves with move-only
payloads, and throwing object/metadata constructors. The installed-library
C++ tutorial demonstrates continued parent progression after child creation.

The initial container stores completion as application metadata; a hybrid
scheduler is not part of these tests. Later capabilities must retain the
distinction between progression inside an object and derivation of a new object.
