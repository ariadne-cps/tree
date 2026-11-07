cmake_minimum_required(VERSION 3.15)
project(Tree VERSION 1.0)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)

if(NOT TARGET ariadne-build)
    find_package(Git REQUIRED)
    # Keep initial Build checkout output in English.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "LC_ALL=C"
                "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
                submodule update --init -- ss/bui
        RESULT_VARIABLE _result)
    if(_result)
        message(FATAL_ERROR "Cannot initialize build submodule")
    endif()
    include("${CMAKE_CURRENT_SOURCE_DIR}/ss/bui/cmake/ProjectOptions.cmake")
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Child commits guarantee their own internal consistency.
    # Keep cross-branch comparisons where the same dependency is shared.
    require_same_dependency_commit(Build CHAINS
        "build" "foundation|build" "threading|build" "testing|build")
    require_same_dependency_commit(Testing CHAINS
        "testing" "foundation|testing" "threading|testing")
endif()
setup_project()
include_directories("${PROJECT_SOURCE_DIR}/include")
add_subdirectory(ss)
add_library(tree INTERFACE)
target_include_directories(tree INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/ariadne-tree>)
target_link_libraries(tree INTERFACE foundation threading)
add_subdirectory(src)
target_link_libraries(TREE_SRC PRIVATE tree)
ariadne_register_public_headers(
    TARGET tree
    DIRECTORIES "${PROJECT_SOURCE_DIR}/include/tree"
    FILES "${PROJECT_SOURCE_DIR}/include/ariadne-tree.hpp")
setup_project_library(TARGET ariadne-tree OBJECTS TREE_SRC LINK_LIBRARIES tree)
setup_standalone_project_tests(TARGET ariadne-tree
    TEST_DIRECTORY tests TEST_TARGET tests
    SOURCES "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
if(BUILD_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    ariadne_install_dependency_bundle(TARGET tree DESTINATION include/ariadne-tree)
endif()
