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
#include <vector>

#include "ariadne-tree.hpp"
using namespace Ariadne;
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

    struct TrajectoryMetadata {
        bool complete;
        std::size_t origin;
    };
    DerivationTree<std::vector<double>, TrajectoryMetadata> trajectories;
    const auto root = trajectories.create_root(std::vector<double>{0.0, 1.0}, TrajectoryMetadata{false, 0});
    const auto child = trajectories.add_child(root, std::vector<double>{1.5}, TrajectoryMetadata{false, 1});
    // The parent keeps progressing even after generating another trajectory.
    trajectories.object(root).push_back(2.0);
    trajectories.metadata(root).complete = true;
    if (trajectories.object(root).size() != 3 || trajectories.metadata(child).complete)
        return 4;
    if (trajectories.parent(child) != root || trajectories.children(root).front() != child)
        return 5;
    if (trajectories.object(root)[trajectories.metadata(child).origin] != 1.0)
        return 6;
    std::cout << "Ariadne Tree tutorial passed.\n";
    return 0;
}
