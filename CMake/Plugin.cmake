# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
function(Plugin target_name current_dir)
    # GLFW
    if(NOT TARGET glfw)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
        add_subdirectory(${current_dir}/External/glfw ${CMAKE_CURRENT_BINARY_DIR}/External/glfw EXCLUDE_FROM_ALL)	
        nos_get_targets(NOSDISPLAY_GLFW_TARGETS "${CMAKE_CURRENT_BINARY_DIR}/External/glfw")
        nos_group_targets("${NOSDISPLAY_GLFW_TARGETS}" "External")
    endif()


    # NvAPI
    # ---
    # External/nvapi is a submodule that contains the headers and libraries for NvAPI.
    set(NVAPI_INCLUDE_DIR "${current_dir}/External/")
    set(NVAPI_LIB_DIR "${current_dir}/External/nvapi/amd64")
    set(NVAPI_LIBS "nvapi64")

    add_library(nvapi INTERFACE)
    target_include_directories(nvapi INTERFACE ${NVAPI_INCLUDE_DIR})
    target_link_directories(nvapi INTERFACE ${NVAPI_LIB_DIR})
    target_link_libraries(nvapi INTERFACE ${NVAPI_LIBS})

    nos_group_targets("nvapi" "External")

    set(CMAKE_DEBUG_POSTFIX "")

    target_link_libraries(${target_name} PRIVATE nvapi glfw)
    nos_group_targets(${target_name} "NOS Plugins")
endfunction()