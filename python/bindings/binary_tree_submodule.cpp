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
