import gc
import runpy
import weakref
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from pyariadne_tree import DerivationTree, DerivationTreeNodeId


@dataclass
class Series:
    values: list[int] = field(default_factory=list)


@dataclass
class Info:
    label: str
    complete: bool = False


def test_generic_payloads_and_mutation():
    tree = DerivationTree()
    assert tree.empty()
    assert len(tree) == tree.size() == 0
    assert tree.root() is None
    data, info = Series([1, 2]), Info("source")
    root = tree.create_root(object=data, metadata=info)
    assert tree.root() == root
    assert tree.parent(root) is None
    assert tree.object(root) is data
    assert tree.metadata(root) is info
    tree.object(root).values.append(3)
    tree.metadata(root).complete = True
    assert data.values == [1, 2, 3]
    assert info.complete
    with pytest.raises(RuntimeError):
        tree.create_root(None, None)
    assert tree.size() == 1
    # The binding is not specialized to the tutorial's dataclasses.
    child = tree.add_child(root, [4], {"complete": False})
    tree.object(child).append(5)
    tree.metadata(child)["complete"] = True
    assert tree.object(child) == [4, 5]
    assert tree.metadata(child) == {"complete": True}
    none_child = tree.add_child(root, None, None)
    assert tree.object(none_child) is None
    assert tree.metadata(none_child) is None


def test_genealogy_is_ordered_and_collections_are_snapshots():
    tree = DerivationTree()
    root = tree.create_root(0, "root")
    first = tree.add_child(root, 1, "first")
    snapshot = tree.children(root)
    second = tree.add_child(root, 2, "second")
    grandchild = tree.add_child(first, 3, "grandchild")
    assert tree.children(root) == [first, second]
    assert snapshot == [first]
    snapshot.clear()
    assert tree.children(root) == [first, second]
    assert tree.parent(grandchild) == first
    assert tree.ancestors(grandchild) == [first, root]
    assert tree.ancestors(root) == []
    assert tree.descendants(root) == [first, grandchild, second]
    assert tree.descendants(grandchild) == []


def test_pruning_and_regrowth_keep_old_ids_invalid():
    tree = DerivationTree()
    root = tree.create_root(Series([1, 2]), Info("source", True))
    data, info = tree.object(root), tree.metadata(root)
    child_data, child_info = Series([3]), Info("old")
    child_data_ref, child_info_ref = weakref.ref(child_data), weakref.ref(child_info)
    child = tree.add_child(root, child_data, child_info)
    grandchild = tree.add_child(child, Series([4]), Info("grandchild"))
    del child_data, child_info
    assert tree.prune(root) == 2
    assert child_data_ref() is None
    assert child_info_ref() is None
    assert tree.root() == root
    assert tree.object(root) is data
    assert tree.metadata(root) is info
    assert info.complete
    assert len(tree) == 1
    assert tree.prune(root) == 0
    for old_id in (child, grandchild):
        assert not tree.contains(old_id)
        for lookup in (tree.object, tree.metadata, tree.parent, tree.children,
                       tree.ancestors, tree.descendants, tree.prune):
            with pytest.raises(IndexError):
                lookup(old_id)
        with pytest.raises(IndexError):
            tree.add_child(old_id, None, None)
    retained_child = tree.add_child(root, Series([5]), Info("retained"))
    retained_reference = tree.object(retained_child)
    for index in range(50):
        newest = tree.add_child(retained_child, index, None)
        assert newest != child and newest != grandchild
        assert not tree.contains(child) and not tree.contains(grandchild)
        assert tree.prune(retained_child) == 1
        assert not tree.contains(newest)
    assert tree.object(retained_child) is retained_reference
    assert tree.children(root) == [retained_child]


def test_returned_payload_reference_survives_pruning():
    tree = DerivationTree()
    root = tree.create_root(None, None)
    child = tree.add_child(root, Series([3]), Info("child"))
    retained_reference = tree.object(child)
    tree.prune(root)
    assert not tree.contains(child)
    assert retained_reference.values == [3]


def test_invalid_foreign_and_expired_identifiers():
    tree = DerivationTree()
    other = DerivationTree()
    root = tree.create_root(None, None)
    foreign = other.create_root(None, None)
    invalid = DerivationTreeNodeId()
    assert invalid == DerivationTreeNodeId()
    assert root == tree.root() and root != foreign
    assert root != None  # noqa: E711 -- exercise comparison to another Python type
    with pytest.raises(TypeError):
        hash(root)
    for node_id in (invalid, foreign):
        assert not tree.contains(node_id)
        with pytest.raises(IndexError):
            tree.prune(node_id)
    with pytest.raises(TypeError):
        tree.contains(0)
    del other
    gc.collect()
    assert not tree.contains(foreign)
    assert tree.size() == 1


def test_python_cycles_are_collectable():
    tree = DerivationTree()
    data, metadata = Series([1]), Info("cycle")
    data.tree = tree
    metadata.tree = tree
    root = tree.create_root(data, metadata)
    child_data = Series([2])
    child_data.tree = tree
    tree.add_child(root, child_data, metadata)
    tree_ref = weakref.ref(tree)
    references = [weakref.ref(data), weakref.ref(metadata), weakref.ref(child_data)]
    del tree, data, metadata, child_data
    gc.collect()
    assert tree_ref() is None
    assert all(reference() is None for reference in references)


def test_gc_handles_uninitialized_instances():
    tree = DerivationTree.__new__(DerivationTree)
    reference = weakref.ref(tree)
    gc.collect()
    del tree
    gc.collect()
    assert reference() is None


def test_finalizers_observe_completed_pruning():
    tree = DerivationTree()
    root = tree.create_root(None, None)
    observations = []

    class Payload:
        def __del__(self):
            observations.append((tree.size(), tree.children(root)))
            tree.add_child(root, "regrown by finalizer", None)

    removed = tree.add_child(root, Payload(), None)
    assert tree.prune(root) == 1
    assert observations == [(1, [])]
    assert not tree.contains(removed)
    assert tree.object(tree.children(root)[0]) == "regrown by finalizer"


def test_python_tutorial_demonstrates_the_cpp_scenario(capsys):
    tutorial = Path(__file__).resolve().parents[2] / "tutorial" / "tutorial_tree.py"
    state = runpy.run_path(str(tutorial))
    tree, root = state["samples"], state["root_id"]
    doubled = state["doubled_id"]
    assert tree.size() == 4
    assert tree.object(root).values == [1, 2, 3]
    assert tree.metadata(root).complete
    assert tree.object(doubled).values == [4, 6, 8]
    assert not tree.metadata(doubled).complete
    assert state["removed_count"] == 1
    assert not state["old_node_exists"]
    assert not tree.contains(state["offset_id"])
    assert [tree.metadata(node).label for node in tree.descendants(root)] == [
        "doubled", "replacement", "tripled"
    ]
    assert tree.object(tree.children(doubled)[0]).values == [9]
    assert state["path_enabled"] and state["is_subset"]
    captured = capsys.readouterr()
    assert captured.out == captured.err == ""
