execute_process(COMMAND "${Python3_EXECUTABLE}" -m pytest --version
    RESULT_VARIABLE PYTEST_result OUTPUT_QUIET ERROR_QUIET)
if(PYTEST_result EQUAL 0)
    set(PYTHON_TESTS ON)
else()
    if(PYTHON_ENFORCE_TESTS)
        message(FATAL_ERROR "pytest missing for ${Python3_EXECUTABLE}")
    else()
        message(WARNING "pytest missing, skipping Python tests")
    endif()
endif()
if(PYTHON_TESTS)
    add_custom_target(test_python_tree
        COMMAND "${Python3_EXECUTABLE}" -m pytest
            "${CMAKE_CURRENT_SOURCE_DIR}/test_tree.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME test_python_tree
        COMMAND "${Python3_EXECUTABLE}" -m pytest
            "${CMAKE_CURRENT_SOURCE_DIR}/test_tree.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(TEST test_python_tree PROPERTY
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:pyariadne>")
    set_property(TEST test_python_tree PROPERTY LABELS "python")
    add_dependencies(test_python_tree pyariadne)
    add_dependencies(tests pyariadne)
endif()
