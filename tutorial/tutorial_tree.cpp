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
 *  @brief Demonstrate binary-tree construction and set operations.
 */

#include <iostream>
#include "ariadne-tree.hpp"
using namespace Ariadne;
int main() {
    BinaryTreeNode prefixes(false);
    prefixes.add_enabled(BinaryWord("01"));
    if(!prefixes.is_enabled(BinaryWord("010")) || prefixes.is_enabled(BinaryWord("11"))) return 1;
    BinaryTreeNode all(true);
    all.split();
    if(!BinaryTreeNode::subset(&prefixes,&all) || BinaryTreeNode::subset(&all,&prefixes)) return 2;
    all.recombine();
    if(!all.is_leaf() || !all.is_enabled()) return 3;
    std::cout << "Ariadne Tree tutorial passed.\n";
    return 0;
}
