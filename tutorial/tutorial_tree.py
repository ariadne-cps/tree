#!/usr/bin/env python3
from pyariadne_tree import BinaryTreeNode

tree = BinaryTreeNode(False)
tree.add_enabled("01")
assert tree.is_enabled_at("0110")
assert not tree.is_enabled_at("11")
full = BinaryTreeNode(True)
full.split()
assert tree.subset_of(full)
assert not full.subset_of(tree)
full.recombine()
assert full.is_leaf() and full.is_enabled()
print("Ariadne Tree Python tutorial passed.")
