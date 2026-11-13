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

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ariadne-tree.hpp"
using namespace Ariadne;

namespace {

/** @brief A small application object: an ordered sequence of integer samples. */
struct SampleSeries {
    std::vector<int> values;
};

/** @brief Application metadata that the generic tree stores without interpreting it. */
struct SampleMetadata {
    std::string label;
    bool complete;
};

using SampleTree = DerivationTree<SampleSeries, SampleMetadata>;

/** @brief Stop the example with a useful diagnostic if its contract is violated. */
void require_example(bool condition, const char* message) {
    if (!condition)
        throw std::logic_error(message);
}

/** @brief Read payloads and parent links through the const tree interface. */
void print_sample_node(const SampleTree& tree, SampleTree::NodeId id) {
    const auto& info = tree.metadata(id);
    std::cout << "  " << info.label << " [";
    const auto& values = tree.object(id).values;
    for (std::size_t sample_index = 0; sample_index < values.size(); ++sample_index) {
        if (sample_index != 0)
            std::cout << ", ";
        std::cout << values[sample_index];
    }
    std::cout << "] " << (info.complete ? "complete" : "needs work");
    if (const auto parent_id = tree.parent(id))
        std::cout << "; parent=" << tree.metadata(*parent_id).label;
    else
        std::cout << "; root";
    std::cout << '\n';
}

/** @brief Show a subtree in depth-first preorder, including its retained root. */
void print_sample_tree(const SampleTree& tree, SampleTree::NodeId root_id) {
    print_sample_node(tree, root_id);
    for (const auto node_id: tree.descendants(root_id))
        print_sample_node(tree, node_id);
}

/** @brief Demonstrate object progression, genealogy, local completion and regrowth. */
void run_derivation_tree_example() {
    std::cout << "\nDerivationTree: sample series and node metadata\n";

    // 1. Select exactly two types. Each node holds a SampleSeries and SampleMetadata.
    SampleTree samples;
    require_example(samples.empty() && !samples.root(), "A new tree must be empty");
    SampleSeries initial_series;
    initial_series.values = {1, 2};
    const SampleMetadata initial_info{"source", false};
    const auto root_id = samples.create_root(initial_series, initial_info);
    require_example(samples.root() == root_id, "The created node must be the root");

    // 2. add_child records derivation; it does not copy or freeze the parent object.
    const int source_value = samples.object(root_id).values.back();
    const auto doubled_id = samples.add_child(root_id, SampleSeries{{2 * source_value}}, SampleMetadata{"doubled", false});
    const auto tripled_id = samples.add_child(root_id, SampleSeries{{3 * source_value}}, SampleMetadata{"tripled", false});

    // 3. Appending data progresses the same object without creating a tree node.
    auto& source_series = samples.object(root_id);
    source_series.values.push_back(3);
    samples.object(doubled_id).values.push_back(2 * source_series.values.back());
    samples.metadata(root_id).complete = true;
    require_example(samples.size() == 3, "Appending samples must not add nodes");
    require_example(!samples.metadata(doubled_id).complete, "Parent completion must not complete its children");

    // 4. A child can generate its own child. Child order and ancestry are explicit.
    const auto offset_id = samples.add_child(doubled_id, SampleSeries{{samples.object(doubled_id).values.front() + 1}}, SampleMetadata{"offset", false});
    samples.metadata(doubled_id).complete = true;
    require_example(samples.children(root_id).size() == 2 && samples.children(root_id)[0] == doubled_id && samples.children(root_id)[1] == tripled_id, "Children must retain insertion order");
    const std::vector<SampleTree::NodeId> expected_ancestors{doubled_id, root_id};
    require_example(samples.ancestors(offset_id) == expected_ancestors, "Ancestors must run from immediate parent to root");
    std::cout << "\nBefore pruning (" << samples.size() << " nodes):\n";
    print_sample_tree(samples, root_id);
    std::cout << "  offset ancestors:";
    for (const auto ancestor_id: samples.ancestors(offset_id))
        std::cout << ' ' << samples.metadata(ancestor_id).label;
    std::cout << '\n';

    // 5. prune retains its boundary and removes only strict descendants.
    // The source reference above and the tripled sibling both remain valid.
    const auto removed_count = samples.prune(doubled_id);
    require_example(removed_count == 1 && samples.size() == 3, "Pruning doubled must remove only offset");
    require_example(samples.contains(doubled_id) && samples.contains(tripled_id), "Pruning must retain the boundary and its sibling");
    require_example(!samples.contains(offset_id), "A pruned ID must become invalid");
    require_example(samples.object(doubled_id).values == std::vector<int>{4, 6} && samples.metadata(doubled_id).complete, "Pruning must preserve boundary data and metadata");
    require_example(source_series.values == std::vector<int>{1, 2, 3}, "An unrelated retained object must remain unchanged");
    require_example(samples.object(tripled_id).values == std::vector<int>{6}, "The sibling payload must remain unchanged");
    std::cout << "\nAfter prune(doubled), removed " << removed_count << " node:\n";
    print_sample_tree(samples, root_id);

    // 6. Completion is application policy: explicitly reopen work before continuing.
    // add_child remains the growth operation, including after pruning.
    samples.metadata(doubled_id).complete = false;
    samples.object(doubled_id).values.push_back(8);
    const auto replacement_id = samples.add_child(doubled_id, SampleSeries{{samples.object(doubled_id).values.back() + 1}}, SampleMetadata{"replacement", false});
    require_example(replacement_id != offset_id && !samples.contains(offset_id), "Regrowth must never revive an old ID");
    require_example(samples.parent(replacement_id) == doubled_id, "Regrowth must use the retained boundary");
    std::cout << "\nAfter regrowth (" << samples.size() << " nodes):\n";
    print_sample_tree(samples, root_id);

    // IDs are opaque handles, not positions to sort or increment. Guard uncertain
    // IDs with contains(); accessing the old offset ID raises std::out_of_range.
    bool rejected_old_id = false;
    try {
        (void)samples.object(offset_id);
    } catch (const std::out_of_range&) {
        rejected_old_id = true;
        std::cout << "  Old offset ID is still invalid after regrowth.\n";
    }
    require_example(rejected_old_id, "Access through a pruned ID must be rejected");
}

} // namespace

int main() {
    BinaryTreeNode prefixes(false);
    prefixes.add_enabled(BinaryWord("01"));
    if (!prefixes.is_enabled(BinaryWord("010")) || prefixes.is_enabled(BinaryWord("11")))
        return 1;
    BinaryTreeNode all(true);
    all.split();
    if (!BinaryTreeNode::subset(&prefixes, &all) || BinaryTreeNode::subset(&all, &prefixes))
        return 2;
    all.recombine();
    if (!all.is_leaf() || !all.is_enabled())
        return 3;

    try {
        run_derivation_tree_example();
    } catch (const std::exception& error) {
        std::cerr << "DerivationTree tutorial failed: " << error.what() << '\n';
        return 4;
    }
    std::cout << "Ariadne Tree tutorial passed.\n";
    return 0;
}
