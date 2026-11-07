#include "pybind11.hpp"
void binary_tree_submodule(pybind11::module& module);
void tree_module(pybind11::module& module) { binary_tree_submodule(module); }
