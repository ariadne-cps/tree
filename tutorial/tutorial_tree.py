#!/usr/bin/env python3
from pyariadne_tree import BinaryTreeNode

# Enable one binary prefix and query membership.
tree = BinaryTreeNode(False)
tree.add_enabled("01")
prefix_member = tree.is_enabled_at("0110")
other_member = tree.is_enabled_at("11")

# Compare represented prefix sets, then reduce the full tree.
full = BinaryTreeNode(True)
full.split()
is_subset = tree.subset_of(full)
reverse_subset = full.subset_of(tree)
full.recombine()
leaf = full.is_leaf()
enabled = full.is_enabled()
