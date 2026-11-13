/***************************************************************************
 *            python/bindings/tree_module.cpp
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

/** @file python/bindings/tree_module.cpp
 *  @brief Register the Ariadne Tree Python module.
 */

#include "pybind11.hpp"
void binary_tree_submodule(pybind11::module& module);
void derivation_tree_submodule(pybind11::module& module);
void tree_module(pybind11::module& module) {
    binary_tree_submodule(module);
    derivation_tree_submodule(module);
}
