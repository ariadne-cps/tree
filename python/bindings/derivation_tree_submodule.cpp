/***************************************************************************
 *            python/bindings/derivation_tree_submodule.cpp
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

/** @file python/bindings/derivation_tree_submodule.cpp
 *  @brief Bind generic derivation trees owning Python object/metadata references.
 */

#include <exception>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "pybind11.hpp"
#include "tree/derivation_tree.hpp"

namespace py = pybind11;

namespace {

using ObjectTree = Ariadne::DerivationTree<py::object, py::object>;
using NodeId = ObjectTree::NodeId;

/** @brief Python ownership adapter; the genealogy is provided by the C++ tree. */
class PythonDerivationTree {
    std::unique_ptr<ObjectTree> _tree = std::make_unique<ObjectTree>();

  public:
    /** @brief Access the live core for ordinary bound operations. */
    ObjectTree& tree() noexcept { return *_tree; }
    /** @brief Access the live core for reading and Python GC traversal. */
    const ObjectTree& tree() const noexcept { return *_tree; }

    /** @brief Prune while delaying Python finalizers until the core is consistent.
     * @param node_id Retained pruning boundary.
     * @return Number of removed descendants.
     * @note The temporary references are released only after C++ pruning finishes.
     * Python finalizers may then inspect or modify the completed tree operation.
     */
    std::size_t prune(NodeId node_id) {
        const auto removed_ids = _tree->descendants(node_id);
        std::vector<std::pair<py::object, py::object>> retained_payloads;
        retained_payloads.reserve(removed_ids.size());
        for (const auto removed_id: removed_ids) {
            retained_payloads.emplace_back(_tree->object(removed_id), _tree->metadata(removed_id));
        }
        return _tree->prune(node_id);
    }

    /** @brief Break Python cycles by replacing the core before releasing its references.
     * @note This is a garbage-collector callback, not a public tree operation.
     */
    void clear_for_gc() {
        auto empty_tree = std::make_unique<ObjectTree>();
        _tree.swap(empty_tree);
    }
};

/** @brief Report all owned Python references without allowing C++ exceptions to escape. */
int traverse_python_tree(PyObject* self_base, visitproc visit, void* arg) noexcept {
    Py_VISIT(Py_TYPE(self_base));
    try {
        // GC can see an instance created by __new__ before __init__ has run.
        if (py::detail::is_holder_constructed(self_base)) {
            const auto* python_tree = py::cast<const PythonDerivationTree*>(py::handle(self_base));
            const auto& tree = python_tree->tree();
            if (const auto root_id = tree.root()) {
                Py_VISIT(tree.object(*root_id).ptr());
                Py_VISIT(tree.metadata(*root_id).ptr());
                for (const auto node_id: tree.descendants(*root_id)) {
                    Py_VISIT(tree.object(node_id).ptr());
                    Py_VISIT(tree.metadata(node_id).ptr());
                }
            }
        }
        return 0;
    } catch (py::error_already_set& error) {
        error.restore();
    } catch (const std::bad_alloc&) {
        PyErr_NoMemory();
    } catch (const std::exception& error) {
        PyErr_SetString(PyExc_RuntimeError, error.what());
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Cannot traverse DerivationTree Python references");
    }
    return -1;
}

/** @brief Release cycles through a fully initialized core, guarding partial construction. */
int clear_python_tree(PyObject* self_base) noexcept {
    try {
        if (py::detail::is_holder_constructed(self_base)) {
            py::cast<PythonDerivationTree&>(py::handle(self_base)).clear_for_gc();
        }
        return 0;
    } catch (py::error_already_set& error) {
        error.restore();
    } catch (const std::bad_alloc&) {
        PyErr_NoMemory();
    } catch (const std::exception& error) {
        PyErr_SetString(PyExc_RuntimeError, error.what());
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Cannot clear DerivationTree Python references");
    }
    return -1;
}

} // namespace

/** @brief Register opaque IDs and the arbitrary-Python-object DerivationTree binding. */
void derivation_tree_submodule(py::module& module) {
    auto node_id_type = py::class_<NodeId>(module, "DerivationTreeNodeId", "Opaque tree-qualified node identifier; default construction is invalid.")
                            .def(py::init<>())
                            .def("__eq__", [](NodeId left, NodeId right) { return left == right; }, py::is_operator())
                            .def("__ne__", [](NodeId left, NodeId right) { return left != right; }, py::is_operator());
    // Equality is value-based; do not inherit address-based object hashing.
    node_id_type.attr("__hash__") = py::none();

    py::class_<PythonDerivationTree>(module, "DerivationTree",
                                     "Generic genealogy storing strong references to arbitrary Python objects and metadata.",
                                     py::custom_type_setup([](PyHeapTypeObject* heap_type) {
                                         auto* type = &heap_type->ht_type;
                                         type->tp_flags |= Py_TPFLAGS_HAVE_GC;
                                         type->tp_traverse = &traverse_python_tree;
                                         type->tp_clear = &clear_python_tree;
                                     }))
        .def(py::init<>())
        .def("empty", [](const PythonDerivationTree& self) { return self.tree().empty(); }, "Return whether the tree has no root.")
        .def("size", [](const PythonDerivationTree& self) { return self.tree().size(); }, "Return the number of live nodes.")
        .def("__len__", [](const PythonDerivationTree& self) { return self.tree().size(); })
        .def("contains", [](const PythonDerivationTree& self, NodeId node_id) { return self.tree().contains(node_id); }, py::arg("node_id"), "Return whether this ID currently resolves in this tree.")
        .def("root", [](const PythonDerivationTree& self) { return self.tree().root(); }, "Return the root ID, or None for an empty tree.")
        .def("create_root", [](PythonDerivationTree& self, py::object object, py::object metadata) { return self.tree().create_root(std::move(object), std::move(metadata)); }, py::arg("object"), py::arg("metadata"), "Create the only root, retaining references to its object and metadata.")
        .def("add_child", [](PythonDerivationTree& self, NodeId parent_id, py::object object, py::object metadata) { return self.tree().add_child(parent_id, std::move(object), std::move(metadata)); }, py::arg("parent"), py::arg("object"), py::arg("metadata"), "Append a child without consuming its parent.")
        .def("object", [](const PythonDerivationTree& self, NodeId node_id) -> py::object { return self.tree().object(node_id); }, py::arg("node_id"), "Return the stored Python object itself, permitting in-place updates.")
        .def("metadata", [](const PythonDerivationTree& self, NodeId node_id) -> py::object { return self.tree().metadata(node_id); }, py::arg("node_id"), "Return the stored Python metadata itself, permitting in-place updates.")
        .def("parent", [](const PythonDerivationTree& self, NodeId node_id) { return self.tree().parent(node_id); }, py::arg("node_id"), "Return the parent ID, or None for the root.")
        .def("children", [](const PythonDerivationTree& self, NodeId node_id) {
            const auto children = self.tree().children(node_id);
            return std::vector<NodeId>(children.begin(), children.end()); }, py::arg("node_id"), "Return a snapshot list of child IDs in insertion order.")
        .def("ancestors", [](const PythonDerivationTree& self, NodeId node_id) { return self.tree().ancestors(node_id); }, py::arg("node_id"), "Return strict ancestors from immediate parent to root.")
        .def("descendants", [](const PythonDerivationTree& self, NodeId node_id) { return self.tree().descendants(node_id); }, py::arg("node_id"), "Return strict descendants in depth-first preorder.")
        .def("prune", &PythonDerivationTree::prune, py::arg("node_id"), "Remove strict descendants, retaining the boundary object and metadata; return the removed count.");
}
