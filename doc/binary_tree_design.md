# BinaryTreeNode design and extraction contract

## Purpose and name

`BinaryTreeNode` means a node **and the subtree rooted at that node**.
The class is not named after the paving domain: neither its representation
nor its operations require geometric cells, grids or paving interfaces.
Keep this public name during extraction to `ariadne-cps/tree`.
The future n-ary tree for trajectory genealogy is a separate abstraction.

## Abstract Boolean interpretation

Paths are finite words over `{0,1}`, represented by `BinaryWord`.
The left child appends 0 and the right child appends 1. An enabled leaf
covers every extension of its path; a disabled leaf excludes all extensions.
Every internal node has both children and an indeterminate
`ValidatedKleenean` value. Leaf membership is Boolean for the path-set
operations. Equivalently, the tree denotes a Boolean function of infinite
bit sequences, constant on the cylinders represented by its leaves.

A tree consisting of a single enabled leaf has the same represented set
as a tree whose two children are enabled leaves. Nonetheless, `operator==`
compares the *representation*, including node state and structure.
For roots of the same abstract domain, semantic equality can be checked
as `subset(a,b) && subset(b,a)`.

## Structural invariants and operations

- Each node is either a leaf with no children or an internal node with
  exactly two ordered children; it exclusively owns its descendants.
- `split()` replaces a leaf with two children of its former state.
  `recombine()` collapses identical sibling leaves; both preserve
  the represented set for Boolean-valued leaves.
- `mince(depth)` refines enabled leaves to the requested depth; disabled
  leaves need not be expanded. `depth()` concerns stored tree depth.
- `add_enabled(path)` inserts a covered prefix; adding a descendant of
  an already enabled ancestor does not change the represented set.
- `restrict(a,b)` and `remove(a,b)` update `a` by intersection and
  difference. `intersect(a,b)` and `subset(a,b)` are read-only tests.
  Both roots must refer to the same abstract binary domain.
- `set()` and `make_leaf()` discard an existing subtree.
  `tree_to_binary_words()` serializes structure and Boolean leaf flags
  in depth-first, left-before-right order; it assumes determinate leaves.
- `prepend_tree(path,root)` transfers ownership of `root` to a newly
  allocated ancestor. It currently assumes the path is nonempty; callers
  must meet that precondition.

## Lifetime, ownership and concurrency

Copy construction and assignment make deep copies. The destructor
recursively destroys descendants. `left_node()`, `right_node()`, and
`child_node()` return **borrowed raw pointers**; changing or recombining
the tree can invalidate them. No persistent handles or shared subtree
ownership are provided.

`BinaryTreeNode` is a sequential building block; simultaneous reads
and mutations of overlapping nodes are not guaranteed safe. Locking
a root only during writes does not protect a borrowed child pointer
used after unlocking. A later owning `BinaryTree` abstraction (or a
domain-specific owner such as `GridTreePaving`) can provide locking,
snapshots and lifetime-safe operations. Algorithms may parallelize
independent subtrees, but concurrency and parallelism do not require
a mutex on every node. None of those new guarantees is implemented here.

## Minimal dependency boundary

`foundation/logical.hpp` supplies `ValidatedKleenean` and associated
logical operations. `utility/binary_word.hpp` supplies path words;
Utility also supplies macros and basic types. The node has no direct
dependencies on `function/functional.hpp`, `set/list_set.hpp` or paving
types. `tree` can therefore depend on Foundation (and transitively Utility).

## Extraction plan and verification

The node and its representation-focused tests are owned by `tree`.
Paving continues to test its own geometric consumers.
Do not alter its public name, raw-pointer contract, or membership
semantics incidentally during the move. The regression suite
`tests/test_binary_tree.cpp` includes grid-independent path tests;
other paving consumers remain tested in their existing suites.

Cross-platform compilation and sanitizers remain necessary to validate
the include cleanup and eventual migration; documentation is not a
substitute for a successful build.

## Threading dependency and implementation boundary

The Tree module directly depends on Ariadne Threading, so future tree owners
and algorithms can reuse the shared threading infrastructure rather than
create an independent execution or synchronization framework. This dependency
does **not** make `BinaryTreeNode` thread-safe: the existing node, ownership,
and borrowed-pointer semantics remain unchanged. Any concurrency contract must
be provided and tested by the container or algorithm using the nodes.

## 2026-11-09 - Opt-in internal recombination benchmark

The bounded `BinaryTreeNode::recombine_parallel()` operation is measured
by `benchmark_binary_tree_parallel` in standalone Tree builds. Tree pins
a new, development-only `ss/bmk` Benchmarking submodule compatible with
the same Build revision. Benchmarking is **not** linked into the installed
production library.

The manual benchmark compares identical deterministic, deeply minc-ed
source trees reduced by either the original serial `recombine()` or
bounded fork/join with shared `ThreadManager` concurrency 2, 4, 8.
Each configuration samples with `Benchmarking::Runner`; `Report`
shows absolute medians, speedup and efficiency. The deep copy before
each sample and structural oracle comparisons after each sample occur
outside the timed interval. A semantic mismatch aborts the experiment.
Results are not yet available: neither speedup nor efficiency is assumed.
No performance benchmark is run in CI.


## 2026-11-09 - Measured parallel recombination and adaptive fork fanout

**Operator-supplied manual Release measurements:** Ariadne Benchmarking
`benchmark_binary_tree_parallel --depths 12,14,16 --workers 0,2,4,8
--samples 7`. The benchmark checked exact structural equality to
serial recombination outside every timed sample.

| Input tree depth | Serial median ms | 2-worker speedup | 4-worker speedup | 8-worker speedup |
| --- | ---: | ---: | ---: | ---: |
| 12 | 0.238750 | 1.473x | 2.345x | 2.418x |
| 14 | 0.704459 | 2.031x | 3.828x | 3.771x |
| 16 | 1.953750 | 2.128x | 4.252x | 4.177x |

At depth 16 the four-worker median was 0.459542 ms, a reduction
of about 76.5% against 1.953750 ms serial. Four and eight worker
results are nearly identical on the larger cases. This has a
specific implementation explanation: the previous fixed two-level
fork budget exposed at most four independent leaf-subtree jobs,
regardless of the ThreadManager pool size. Worker counts denote
configured pool threads, not measured concurrently active cores.
An observed speedup beyond a worker count is a benchmark point
estimate, not evidence of physically superlinear scaling; cache,
scheduling, allocations and sample variability remain plausible.

**Follow-up source change:** retain the previous two fork levels for
concurrency 2-4, but adapt the recursion budget upward when the
configured pool contains more than four workers. Every additional
level doubles the maximum independent leaf-subtree partitions.
Cap the budget at five levels to avoid unrestricted tiny-task
proliferation. All tasks share ThreadManager and use TaskGroup
fork/join; no nested pools, node locks or changed recombine
semantics are introduced. Extend the semantic regression across
0/2/4/8 workers where supported.

This is a new *performance hypothesis*. The table above is the
**fixed-two-level baseline**, not a measurement of adaptive fanout.
A repeated manual experiment on the same hardware with the same
parameters is needed to decide whether eight workers now help.
No benchmark execution or performance threshold is added to CI.


## 2026-11-09 - Measured adaptive fork fanout: revert to two levels

**Second operator-run manual benchmark**, same deterministic input
family, depths 12/14/16, 7 measured samples, workers 0/2/4/8.
Every timed operation was followed by structural verification against
the same sequential reference. Absolute median milliseconds:

| Depth | Workers | Fixed 2-level fork | Adaptive fork | Adaptive versus fixed |
| --- | ---: | ---: | ---: | ---: |
| 12 | 0 | 0.238750 | 0.238209 | -0.2% |
| 12 | 2 | 0.162042 | 0.170708 | +5.3% |
| 12 | 4 | 0.101791 | 0.130708 | +28.4% |
| 12 | 8 | 0.098750 | 0.161333 | +63.4% |
| 14 | 0 | 0.704459 | 0.700333 | -0.6% |
| 14 | 2 | 0.346792 | 0.347709 | +0.3% |
| 14 | 4 | 0.184042 | 0.180584 | -1.9% |
| 14 | 8 | 0.186792 | 0.243916 | +30.6% |
| 16 | 0 | 1.953750 | 1.863125 | -4.6% |
| 16 | 2 | 0.918083 | 0.889458 | -3.1% |
| 16 | 4 | 0.459542 | 0.593875 | +29.2% |
| 16 | 8 | 0.467792 | 0.466292 | -0.3% |

At concurrency 8, the adaptive code exposed eight leaf-subtree
partitions instead of four. There was no meaningful depth-16 gain;
the depth-12 and depth-14 measurements regressed by approximately
63% and 31% respectively. This is consistent with excess fork/join
overhead for too-small tasks. **It does not demonstrate that the
fork budget caused all observed differences:** notably, the 4-worker
implementation followed the same two-level branch in both versions,
yet depth-16 timings differed by 29%, evidence of cross-run noise,
runtime scheduling variation, or another uncontrolled difference.

**Decision:** restore the previously measured, simpler two-level
fork budget for every concurrency >=2; preserve the extended
multiworker structural regression and manual Benchmarking executable.
Do not add automated performance thresholds or run the benchmark in
CI. A further 8-way variant would require substantially larger
inputs and an adaptive work threshold justified by repeated
measurements, not merely an extra level chosen from pool size.
Next focus is the ROBDD internal DAG algorithm (canonical table
synchronization and ITE branches), with its own standalone
Benchmarking experiment before performance claims.


## 2026-11-09 - Benchmark CLI correction: honor the caller's concurrency

The first manual tree benchmark had a misleading interface: it added
the standard `threading_command_line_contribution()` (including
`--concurrency`), but overrode that value inside `measure()` by
calling `ThreadManager::set_concurrency(workers)` for each item of
the benchmark-specific `--workers` sweep. Consequently an explicitly
provided `--concurrency` was ignored by the experiment.

**Corrected protocol:** remove the redundant `--workers` argument,
all internal pool reconfiguration and its restore guard. A single
process now honors the standard `--concurrency` configured by
Threading and benchmarks, for each selected depth, the sequential
`recombine()` and `recombine_parallel()` on identical deep copies
under that **same** runtime configuration. Input cloning and
post-operation structural equality checks remain untimed and the
measurement/reporting implementation remains wholly within Ariadne
Benchmarking (`Runner`, `Statistics`, `Report`).
The sequential reference is measured even when concurrency >0; the
parallel implementation naturally falls back to the sequential
algorithm when concurrency <2. Record both methods separately with
`method` and `concurrency` machine-readable fields.

An explicit 0/2/4/8 scaling sweep is now done with **four manual
process invocations** (`--concurrency 0`, etc.). The per-run reported
speedup is the ratio of serial and parallel medians at identical
concurrency, reducing hidden pool changes but not implying that
independently measured series are free from system noise. The prior
two-series results remain historic observations from the old
in-process sweep protocol; no new speedup is claimed without
re-running the corrected benchmark. No CI workflow changes or
automatic execution of performance benchmarks.
