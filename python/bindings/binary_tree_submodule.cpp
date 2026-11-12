/***************************************************************************
 *            python/bindings/binary_tree_submodule.cpp
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

/** @file python/bindings/binary_tree_submodule.cpp
 *  @brief Expose binary-tree node operations to Python.
 */

#include <memory>
#include <string>
#include "pybind11.hpp"
#include "tree/binary_tree.hpp"
using namespace Ariadne;

void binary_tree_submodule(pybind11::module& module) {
    pybind11::class_<BinaryTreeNode>(module,"BinaryTreeNode")
        .def(pybind11::init([](Bool enabled) {
            return std::make_unique<BinaryTreeNode>(enabled);
        }),pybind11::arg("enabled")=false)
        .def("is_leaf",&BinaryTreeNode::is_leaf)
        .def("is_enabled",static_cast<Bool(BinaryTreeNode::*)() const>(&BinaryTreeNode::is_enabled))
        .def("is_disabled",&BinaryTreeNode::is_disabled)
        .def("has_enabled",&BinaryTreeNode::has_enabled)
        .def("all_enabled",&BinaryTreeNode::all_enabled)
        .def("is_enabled_at",[](BinaryTreeNode const& node,std::string const& path) {
            return node.is_enabled(BinaryWord(path));
        })
        .def("add_enabled",[](BinaryTreeNode& node,std::string const& path) {
            node.add_enabled(BinaryWord(path));
        })
        .def("split",&BinaryTreeNode::split)
        .def("mince",&BinaryTreeNode::mince)
        .def("recombine",&BinaryTreeNode::recombine)
        .def("depth",&BinaryTreeNode::depth)
        .def("set",&BinaryTreeNode::set)
        .def("subset_of",[](BinaryTreeNode const& a,BinaryTreeNode const& b) {
            return BinaryTreeNode::subset(&a,&b);
        })
        .def("intersects",[](BinaryTreeNode const& a,BinaryTreeNode const& b) {
            return BinaryTreeNode::intersect(&a,&b);
        })
        .def("__eq__",[](BinaryTreeNode const& a,BinaryTreeNode const& b) {
            return a==b;
        })
        .def("__str__",[](BinaryTreeNode const& node) {
            return node.node_to_string();
        });
}
