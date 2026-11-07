if(NOT TARGET foundation)
    ariadne_add_submodule(foundation)
endif()
if(NOT TARGET threading)
    ariadne_add_submodule(threading)
endif()

ariadne_setup_testing()
