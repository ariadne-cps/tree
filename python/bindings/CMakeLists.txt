include_directories(${Python3_INCLUDE_DIRS})
get_target_property(PYTHON_COMMON_SOURCE_DIR pyariadne-python-common SOURCE_DIR)
add_library(pyariadne-tree-submodule-obj OBJECT
    binary_tree_submodule.cpp
    derivation_tree_submodule.cpp)
target_include_directories(pyariadne-tree-submodule-obj PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PYTHON_COMMON_SOURCE_DIR}/include")
target_link_libraries(pyariadne-tree-submodule-obj PRIVATE
    pyariadne-python-common pybind11::module tree)
set_target_properties(pyariadne-tree-submodule-obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(pyariadne-tree INTERFACE)
target_sources(pyariadne-tree INTERFACE
    $<TARGET_OBJECTS:pyariadne-tree-submodule-obj>)
target_include_directories(pyariadne-tree INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(pyariadne-tree INTERFACE pyariadne-foundation)
ariadne_add_python_module(
    NAME tree MODULE_IMPORT_NAME pyariadne_tree MODULE_SOURCE tree_module.cpp)
