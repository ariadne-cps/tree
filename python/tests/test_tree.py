from pyariadne_tree import BinaryTreeNode

def test_prefix_membership():
    tree = BinaryTreeNode(False)
    assert tree.is_leaf()
    tree.add_enabled("01")
    assert tree.is_enabled_at("01")
    assert tree.is_enabled_at("0101")
    assert not tree.is_enabled_at("11")

def test_semantic_vs_structural_equality():
    full = BinaryTreeNode(True)
    expanded = BinaryTreeNode(True)
    expanded.split()
    assert full != expanded
    assert full.subset_of(expanded)
    assert expanded.subset_of(full)
    expanded.recombine()
    assert full == expanded
