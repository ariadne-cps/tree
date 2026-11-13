#!/usr/bin/env python3
from dataclasses import dataclass, field

from pyariadne_tree import BinaryTreeNode, DerivationTree


# Binary-prefix membership and subtree reduction.
prefixes = BinaryTreeNode(False)
prefixes.add_enabled("01")
path_enabled = prefixes.is_enabled_at("010")
all_paths = BinaryTreeNode(True)
all_paths.split()
is_subset = prefixes.subset_of(all_paths)
all_paths.recombine()


# An application object and its node metadata, matching the two C++ types.
@dataclass
class SampleSeries:
    values: list[int] = field(default_factory=list)


@dataclass
class SampleMetadata:
    label: str
    complete: bool


SampleTree = DerivationTree

# Create a root object and initialize its ordered samples to [1, 2].
samples = SampleTree()
root_id = samples.create_root(SampleSeries(), SampleMetadata("source", False))
samples.object(root_id).values = [1, 2]

# Derive two children from the source sample 2.
source_value = samples.object(root_id).values[-1]
doubled_id = samples.add_child(
    root_id, SampleSeries([2 * source_value]), SampleMetadata("doubled", False)
)
samples.add_child(
    root_id, SampleSeries([3 * source_value]), SampleMetadata("tripled", False)
)

# Continue the existing objects without creating more tree nodes.
source_series = samples.object(root_id)
source_series.values.append(3)
samples.object(doubled_id).values.append(2 * source_series.values[-1])
samples.metadata(root_id).complete = True

# A child can generate its own child; completion remains node-local.
offset_id = samples.add_child(
    doubled_id,
    SampleSeries([samples.object(doubled_id).values[0] + 1]),
    SampleMetadata("offset", False),
)
samples.metadata(doubled_id).complete = True

# Read genealogical relationships. Results are available for inspection.
child_ids = samples.children(root_id)
parent_id = samples.parent(offset_id)
ancestor_ids = samples.ancestors(offset_id)
descendant_ids = samples.descendants(root_id)

# Remove offset, retaining doubled, its [4, 6] samples and its metadata.
removed_count = samples.prune(doubled_id)

# Reopen work explicitly, continue doubled and derive a replacement child.
samples.metadata(doubled_id).complete = False
samples.object(doubled_id).values.append(8)
samples.add_child(
    doubled_id,
    SampleSeries([samples.object(doubled_id).values[-1] + 1]),
    SampleMetadata("replacement", False),
)

# The old offset identifier remains invalid after regrowth.
old_node_exists = samples.contains(offset_id)
