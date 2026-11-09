# Ariadne Tree

[![macOS Status](https://github.com/ariadne-cps/tree/actions/workflows/macos.yml/badge.svg)](https://github.com/ariadne-cps/tree/actions/workflows/macos.yml)
[![Ubuntu Status](https://github.com/ariadne-cps/tree/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/ariadne-cps/tree/actions/workflows/ubuntu.yml)
[![Windows Status](https://github.com/ariadne-cps/tree/actions/workflows/win.yml/badge.svg)](https://github.com/ariadne-cps/tree/actions/workflows/win.yml)
[![Sanitizers Status](https://github.com/ariadne-cps/tree/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/ariadne-cps/tree/actions/workflows/sanitizers.yml)
[![Coverage Status](https://github.com/ariadne-cps/tree/actions/workflows/coverage.yml/badge.svg)](https://github.com/ariadne-cps/tree/actions/workflows/coverage.yml)
[![codecov](https://codecov.io/gh/ariadne-cps/tree/branch/main/graph/badge.svg)](https://codecov.io/gh/ariadne-cps/tree)

Ariadne Tree is the standalone C++20 tree data-structure layer for Ariadne. The repository is intended to house reusable tree representations independently of the domains that consume them. Its current implementation is `BinaryTreeNode`, a binary-tree node owning its rooted subtree and representing Boolean membership of binary paths. The implementation originated in [Ariadne Paving](https://github.com/ariadne-cps/paving), but neither its types nor its operations require geometric pavings.

Although independently maintained and usable, this repository is also part of the [Ariadne repository structure](https://github.com/ariadne-cps/ariadne/blob/main/doc/structure.md).

## Current features

- `BinaryTreeNode` with zero or two ordered children, and independent ownership of both subtrees.
- Enabled and disabled leaves representing Boolean membership of binary prefixes.
- Refinement via `split()` and `mince()`, and representation reduction via `recombine()`.
- Prefix insertion, path membership, intersection, difference, inclusion and intersection predicates.
- Deep copying and serialization of the tree shape and leaf values.
- Standalone C++ regression tests with no grid or paving dependency.
- Optional Python binding exposing `BinaryTreeNode` and its safe value-based operations.
- Installed-library C++ and Python tutorials.
- Ubuntu, macOS, Windows, sanitizer and coverage CI.

`BinaryTreeNode` is **not a synchronized container**. Its child accessors return borrowed pointers, which can become invalid after mutations. Alongside sequential `recombine()`, it offers bounded internal subtree parallelism through `recombine_parallel()` using the shared `ThreadManager`; callers must prevent concurrent access to the same tree while this operation runs. No mutex is added to individual nodes.

The planned n-ary tree for hybrid-evolution trajectory dependencies is **not implemented**. It will be designed as a separate abstraction rather than imposed on `BinaryTreeNode`.

## Layout

- `include/tree/`: public C++ headers; `include/ariadne-tree.hpp` is the umbrella header.
- `src/`: C++ implementations.
- `tests/`: independent C++ regression tests.
- `benchmarks/`: opt-in, standalone performance experiments using Ariadne Benchmarking.
- `python/bindings/`, `python/tests/`: optional Python extension and pytest tests.
- `tutorial/`: standalone C++ and Python examples using an installed Tree module.
- `doc/binary_tree_design.md`: semantics, ownership, invariants, limitations and extraction contract.
- `.github/workflows/`: platform, sanitizers and code-coverage checks.

## Dependencies

The repository contains these direct Git submodules (pinned consistently with the rest of Ariadne):

- `ss/bui` -> [ariadne-cps/build](https://github.com/ariadne-cps/build), shared CMake infrastructure.
- `ss/fou` -> [ariadne-cps/foundation](https://github.com/ariadne-cps/foundation), logical types including `ValidatedKleenean`.
- `ss/thr` -> [ariadne-cps/threading](https://github.com/ariadne-cps/threading), shared threading and task-execution infrastructure for future concurrent tree owners and parallel algorithms.
- `ss/tst` -> [ariadne-cps/testing](https://github.com/ariadne-cps/testing), standalone C++ testing.
- `ss/bmk` -> [ariadne-cps/benchmarking](https://github.com/ariadne-cps/benchmarking), development-only benchmarking tools (not linked into the production Tree library).

[Utility](https://github.com/ariadne-cps/utility), which provides `BinaryWord`, is reached transitively through Foundation. Python bindings use the Python Common and pybind11 infrastructure supplied by the Foundation dependency chain.

There is no direct dependency on Ariadne Function, Set, BDD, Paving or Evolution. Consumers such as Paving depend on Tree, not the reverse. Clone recursively so that submodule dependencies are available.

## Build

```bash
git clone --recurse-submodules https://github.com/ariadne-cps/tree.git
cd tree
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel --target everything
ctest --test-dir build --output-on-failure
```

The project requires CMake and a C++20 compiler. The public CMake module target is `tree`; a standalone build produces the `ariadne-tree` library. Set `-DBUILD_PYTHON_BINDINGS=OFF` for a C++-only build. The optional Python extension requires a compatible Python development environment.

## BinaryTreeNode semantics

A node is the root of its own subtree. The left child corresponds to bit `0` and the right child to bit `1`. An enabled leaf covers **all extensions of its prefix**; a disabled leaf covers none. Internal nodes have two children and an indeterminate `ValidatedKleenean` value.

`operator==` compares stored tree representations, **not** equality of the represented sets. Two different shapes may denote the same Boolean set of paths. When roots refer to the same abstract path domain, use inclusion in both directions to establish semantic equality.

See the [BinaryTreeNode design specification](doc/binary_tree_design.md) for the formal representation, API behavior and ownership contract.

## Python bindings

Python 3.11 or newer with development headers is used when `BUILD_PYTHON_BINDINGS=ON`. The standalone extension is `pyariadne_tree` and exposes `BinaryTreeNode` with Boolean construction, refinement, path membership, prefix insertion and set predicates. Its Python API deliberately does not expose borrowed raw child pointers.

Python tests use pytest. Configure with `-DPYTHON_ENFORCE_TESTS=ON` to require Python and pytest rather than skip unavailable Python tests.

## Tutorials

A standalone C++ tutorial is available in `tutorial/`, built against an **installed** Ariadne Tree library, as in the BDD repository.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel --target everything
cmake --install build-release --prefix /path/to/install
cmake -S tutorial -B tutorial-build -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build tutorial-build
./tutorial-build/tutorial_tree
```

The corresponding Python tutorial is `tutorial/tutorial_tree.py` and imports the installed `pyariadne_tree` extension. CI runs C++/Python tests in Debug and builds installed-library tutorials in Release.

## ThreadSanitizer

```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
    -DTHREAD_SANITIZER=ON -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build-tsan --parallel --target tests
ctest --test-dir build-tsan --output-on-failure
```

ThreadSanitizer checks for data races during the selected tests; it does not imply that concurrent mutation of `BinaryTreeNode` is supported.

## Manual internal-parallelism benchmark

The standalone `benchmark_binary_tree_parallel` executable uses the
standard **`--concurrency N`** option supplied by the Ariadne Threading
command-line component. The benchmark **never changes ThreadManager
concurrency internally** and no longer accepts `--workers`.

For every selected depth it measures both `recombine()` and
`recombine_parallel()` at the *same* configured concurrency.
The first is an explicit sequential baseline independent of pool size.
The second uses the internal shared-pool fork/join algorithm when
concurrency is at least two, and a sequential fallback otherwise.
At `--concurrency 0`, both methods are sequential, as expected.

Measurements use **Benchmarking::Runner** for warmups/samples and
**Benchmarking::Report** for the table. Source cloning and exact
structural verification against the sequential oracle occur outside
the timed region. Output includes absolute median milliseconds,
within-process speedup, parallel efficiency, and machine-readable
`TREE_PARALLEL_RECOMBINE` rows with `method` and `concurrency`.
The implementation is limited to two fork levels (at most four
independent leaf-subtree partitions), based on the prior manual
adaptive-vs-fixed comparison.

Run from the repository root using a Release build:

```bash
git pull --ff-only
git submodule update --init --recursive
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_PYTHON_BINDINGS=OFF
cmake --build build-release --target benchmark_binary_tree_parallel

for concurrency in 0 2 4 8; do
  ./build-release/benchmarks/benchmark_binary_tree_parallel \
    --concurrency "$concurrency" --depths 12,14,16 --samples 7
done
```

`--warmups` and `--summary` remain available from the shared
Benchmarking CLI. A pool size is a configured ThreadManager value, not
a measured number of concurrently active physical cores. For paired
comparisons use the serial and parallel medians of the **same run**;
do not read the superseded `--workers` sweep as the new CLI contract.

This experiment is **not registered with CTest** and is **not executed
by CI**. As before, only compilation of benchmark targets may occur
through standard project build targets.
