# Try to find the standalone Ariadne TREE library.
#
# Once done this will define:
#
#  AriadneTree_FOUND - system has Ariadne TREE
#  ARIADNE_TREE_INCLUDE_DIRS - the Ariadne TREE include directories
#  ARIADNE_TREE_LIBRARIES - libraries required to use Ariadne TREE
#
# and, when found:
#
#  AriadneTree::ariadne-tree - imported library target
#
# Copyright  2026  Luca Geretti

find_library(ARIADNE_TREE_LIBRARY NAMES ariadne-tree)
find_path(ARIADNE_TREE_INCLUDE_DIR ariadne-tree.hpp PATH_SUFFIXES ariadne-tree)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AriadneTree DEFAULT_MSG
  ARIADNE_TREE_LIBRARY
  ARIADNE_TREE_INCLUDE_DIR
)

set(ARIADNE_TREE_FOUND ${AriadneTree_FOUND})

if(AriadneTree_FOUND)
  set(ARIADNE_TREE_INCLUDE_DIRS ${ARIADNE_TREE_INCLUDE_DIR})
  set(ARIADNE_TREE_LIBRARIES ${ARIADNE_TREE_LIBRARY})

  if(NOT TARGET AriadneTree::ariadne-tree)
    add_library(AriadneTree::ariadne-tree UNKNOWN IMPORTED)
    set_target_properties(AriadneTree::ariadne-tree PROPERTIES
      IMPORTED_LOCATION "${ARIADNE_TREE_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ARIADNE_TREE_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(
  ARIADNE_TREE_INCLUDE_DIR
  ARIADNE_TREE_LIBRARY
)
