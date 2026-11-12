# DerivationTree design proposal

Status: design draft; no implementation or public API is claimed by this document.

## Purpose and boundary

Provide a generic n-ary history of objects produced from other objects. Each node
has an object and application-defined metadata. A derivation records why its
outputs were created. The immediate consumer is hybrid evolution, whose domain
contract belongs in `ariadne-cps/evolution/doc/evolution_tree_design.md`.

This is a separate abstraction from `BinaryTreeNode`. That existing class models
Boolean membership of binary prefixes and owns a binary subtree; it does not
provide the derivation history described here. Its ownership, API and semantics
are unaffected by this proposal.

Tree must not depend on Evolution, hybrid enclosures, continuous time, discrete
events, numerical configurations, or trajectory/subset/alternative identifiers.
Those are consumer-defined objects and metadata. Tree does not decide whether
two mathematical sets can be recombined, nor compute their union.

## Requirements and proposed representation

The requested requirements are n-ary children, one structural parent, stable node
identity, an object and metadata on every node, a reason for producing children,
and support for historical editing without renumbering existing descendants.
The following representation is a proposal for satisfying them, including
multi-input recombination without giving a node several structural owners.

Use an owning `DerivationTree<Object, NodeMetadata, DerivationMetadata>`.
The template spelling is provisional. Empty metadata types may be defaults.
The owner stores nodes and derivation records; callers use stable identifiers
rather than addresses or paths.

A node conceptually contains:

- A unique `NodeId` within its owning tree.
- Its `Object` and `NodeMetadata`.
- An optional structural parent and ordered structural children.
- The derivation that produced it, except at the root.
- Its outgoing derivation, if it has been expanded or absorbed.
- Its lifecycle state and, when applicable, a replacement reference.

A derivation conceptually contains:

- Application-defined `DerivationMetadata`, including the reason.
- One or more source node identifiers, with one designated primary source.
- The ordered output node identifiers.

For an ordinary expansion there is one source. Every output has that source as
its structural parent. Child-specific information can be stored in the child's
metadata, since the incoming structural edge is unique. For a recombination
there are several sources and one output; its structural parent is the primary
source. Other sources are absorbed and refer to the same derivation/output.

Thus structural ownership remains a rooted tree. The complete data-dependency
history, including the additional sources, is a directed acyclic graph. Primary
parent traversal must never be mistaken for complete provenance traversal.

## Identity and ownership

`NodeId` is allocated independently of depth, child position, node address and
application metadata. It is immutable and never reused within a tree, including
after invalidation. Handles from different trees cannot be combined. An ID
continues to identify the same historical node even when that node is no longer
eligible for further work.

The owner controls lifetimes. Parents, children and additional source references
are non-owning identifiers, not mutually owning smart pointers. Reallocation of
internal storage must not invalidate IDs. References returned by lookup are
borrowed; their lifetime and concurrent-access contract must be documented.
An initial implementation should not expose mutable topology through them.

The consumer supplies identifiers carried in metadata. Equality of those values
does not imply node identity or ancestry. Tree does not allocate, renumber,
interpret or propagate trajectory, subset, alternative or family identifiers.

## Expansion and lifecycle

A frontier node is an eligible unexpanded node. Being a structural leaf is
insufficient: an absorbed, terminated or invalidated node is not frontier work.
Distinguish at least these lifecycle states:

| State | Meaning |
| --- | --- |
| Frontier | Eligible for an expansion or explicit termination |
| Expanded | Consumed as the primary source of a committed derivation |
| Absorbed | Consumed as an additional source of a multi-input derivation |
| Terminated | Intentionally stopped without a successor |
| Invalidated | Retained historical work that is no longer valid |

Expansion is an owner-level operation. Objects and metadata for the children are
prepared before committing structural changes. The operation either publishes
the complete derivation and all outputs or leaves the owner unchanged. There
must be no observable half-split or half-recombined frontier.

An ordinary expansion accepts one frontier source, a reason, and one or more
output descriptions. A termination is explicit rather than an ambiguous
zero-child expansion. Reasons and cardinality constraints specific to an
application are checked by the consumer's wrapper.

Once committed, historical objects and metadata are treated as immutable during
normal forward processing. Changes such as a new numerical configuration or a
new interpretation of an object should normally produce a successor node.

## Multi-input derivation

A generic multi-input operation records that a new object was produced from a
collection of source objects. Recombination is one application of it. The caller
provides the new object; Tree performs no mathematical merge.

Before commit, require:

1. Distinct, live frontier sources from the same owner.
2. A primary source belonging to that collection.
3. No source is an ancestor of another source.
4. No conflicting committed consumer of a source.
5. No new cycle in the complete dependency relation.

Commit creates the output under the primary source, records all sources, marks
the primary expanded and the remaining sources absorbed, and removes every
source from the active frontier. A worker holding an old source ID must recheck
its state before committing results. Retain absorbed objects and provenance for
inspection and backtracking; absorption is not physical deletion.

The consumer decides which application identifiers the result retains. Selecting
a primary source must be explicit, not dependent on sibling order or execution
order. An implementation may choose a deterministic primary by policy.

## Historical insertion and backtracking

Inserting a new unary node on a structural edge must not renumber the existing
child or any descendant. The node receives a fresh `NodeId`. The affected
derivation relationships and metadata must be replaced consistently, with the
previous relationship retained in edit history if historical revisions are
supported. Reparenting cannot silently change additional source dependencies.

Stable IDs alone do not make old results valid after an edit. A consumer must
either establish that an insertion preserves the meaning of existing objects,
or invalidate and recompute affected work. A topology-only insertion cannot
claim that changed dynamics, configurations or branch decisions have already
been applied downstream.

Invalidation follows all dependency edges, including additional sources of a
multi-input derivation. Invalidating an absorbed source can therefore invalidate
the recombined result and its descendants even though they are outside that
source's structural subtree.

Reopening an earlier frontier requires a coherent edit transaction: invalidate
its obsolete dependants, withdraw the conflicting outgoing derivation from the
active view, and reactivate the selected checkpoint. Do not reuse old IDs for
replacement computations. Whether this uses an explicit revision log or retained
inactive derivations is an implementation decision still to be settled.

## Proposed operations

The following is an API outline, not a finalized C++ interface:

```text
create_root(object, metadata) -> NodeId
lookup(NodeId) -> read-only node view
expand(source, reason, output_descriptions) -> ordered output IDs
derive_from(sources, primary, reason, output_description) -> NodeId
terminate(source, termination_metadata)
insert_between(parent, child, object, metadata, replacement_derivations)
invalidate_dependants(source)
parent(NodeId), children(NodeId), sources(NodeId)
frontier(), ancestors(NodeId), dependency_dependants(NodeId)
```

Traversal must distinguish structural descendants from dependency dependants.
The tree is authoritative for lifecycle state; a scheduling queue is only an
index of pending work and may contain stale IDs. Scheduling priority and
numerical work execution belong outside Tree.

## Concurrency and persistence

The first implementation should have an explicit sequential mutation contract.
Computing candidate objects outside the owner permits later parallel workers
without requiring a mutex in each node. If concurrent publication is added, use
the existing Ariadne Threading facilities and make multi-source validation and
commit indivisible. A generation/revision check is needed when a calculation can
finish after its source has been absorbed or invalidated.

Persisted histories must include objects and metadata through consumer-supplied
serialization, node IDs, derivations, lifecycle states, additional dependencies,
and allocator high-water marks. Restoring a history must not reissue an old ID.
Persistence is a follow-up capability, not a prerequisite for the initial tree.

## Verification and remaining decisions

Implementation tests should exercise unary and n-ary expansion; independent
object/metadata types; identity stability after storage growth and insertion;
complete atomic publication on failure; multi-input absorption across different
subtrees; stale frontier work; cycle and foreign-ID rejection; invalidation
through additional dependencies; and non-reuse of invalidated IDs.

Before implementation, settle exact names, handle representation, object move/
copy requirements, edit-history representation, memory reclamation, and whether
historical insertion ships initially or follows append/invalidate/backtrack.
Do not claim simultaneous mutation, serialization or persistence until their
contracts have implementations and tests.
