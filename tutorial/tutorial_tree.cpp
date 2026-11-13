/***************************************************************************
 *            tutorial/tutorial_tree.cpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne Tree.
 *
 *  Ariadne Tree is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne Tree is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne Tree. If not, see <https://www.gnu.org/licenses/>.
 */

/** @file tutorial/tutorial_tree.cpp
 *  @brief Demonstrate binary-prefix sets and object genealogy.
 */

#include <string>
#include <vector>

#include "ariadne-tree.hpp"

using namespace Ariadne;

int main() {
    // Binary-prefix membership and subtree reduction.
    BinaryTreeNode prefixes(false);
    prefixes.add_enabled(BinaryWord("01"));
    [[maybe_unused]] const bool path_enabled = prefixes.is_enabled(BinaryWord("010"));
    BinaryTreeNode all(true);
    all.split();
    [[maybe_unused]] const bool is_subset = BinaryTreeNode::subset(&prefixes, &all);
    all.recombine();

    // An application object and its node metadata: exactly two template parameters.
    struct SampleSeries {
        std::vector<int> values;
    };
    struct SampleMetadata {
        std::string label;
        bool complete;
    };
    using SampleTree = DerivationTree<SampleSeries, SampleMetadata>;

    // Create a root object containing the ordered samples [1, 2].
    SampleTree samples;
    SampleSeries initial_series;
    initial_series.values = {1, 2};
    const auto root_id = samples.create_root(initial_series, SampleMetadata{"source", false});

    // Derive two children from the source sample 2.
    const int source_value = samples.object(root_id).values.back();
    const auto doubled_id = samples.add_child(root_id, SampleSeries{{2 * source_value}}, SampleMetadata{"doubled", false});
    samples.add_child(root_id, SampleSeries{{3 * source_value}}, SampleMetadata{"tripled", false});

    // Continue the existing objects without creating more tree nodes.
    auto& source_series = samples.object(root_id);
    source_series.values.push_back(3);
    samples.object(doubled_id).values.push_back(2 * source_series.values.back());
    samples.metadata(root_id).complete = true;

    // A child can generate its own child; completion remains node-local.
    const auto offset_id = samples.add_child(doubled_id, SampleSeries{{samples.object(doubled_id).values.front() + 1}}, SampleMetadata{"offset", false});
    samples.metadata(doubled_id).complete = true;

    // Read genealogical relationships. Results are available for inspection.
    [[maybe_unused]] const auto child_ids = samples.children(root_id);
    [[maybe_unused]] const auto parent_id = samples.parent(offset_id);
    [[maybe_unused]] const auto ancestor_ids = samples.ancestors(offset_id);
    [[maybe_unused]] const auto descendant_ids = samples.descendants(root_id);

    // Remove offset, retaining doubled, its [4, 6] samples and its metadata.
    [[maybe_unused]] const auto removed_count = samples.prune(doubled_id);

    // Reopen work explicitly, continue doubled and derive a replacement child.
    samples.metadata(doubled_id).complete = false;
    samples.object(doubled_id).values.push_back(8);
    samples.add_child(doubled_id, SampleSeries{{samples.object(doubled_id).values.back() + 1}}, SampleMetadata{"replacement", false});

    // The old offset identifier remains invalid after regrowth.
    [[maybe_unused]] const bool old_node_exists = samples.contains(offset_id);
}
