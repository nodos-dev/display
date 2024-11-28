# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
cmake_minimum_required(VERSION 3.24.2)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

if (NOT WITH_NODOS_WORKSPACE)
    message(FATAL_ERROR "This repo currently does not support builds without Nodos workspace. "
    "Place this repo under nodos-workspace/Module folder and run cmake -S ./Toolchain/CMake -B Build from workspace root.")
endif()

# Nodos SDK
nos_find_sdk("1.3.0" NOS_PLUGIN_SDK_TARGET NOS_SUBSYSTEM_SDK_TARGET NOS_SDK_DIR)


# GLFW
if(NOT TARGET glfw)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
	set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/glfw EXCLUDE_FROM_ALL)	
	nos_get_targets(NOSDISPLAY_GLFW_TARGETS "${CMAKE_CURRENT_SOURCE_DIR}/External/glfw")
	nos_group_targets("${NOSDISPLAY_GLFW_TARGETS}" "External")
endif()


# NvAPI
# ---
# External/nvapi is a submodule that contains the headers and libraries for NvAPI.
set(NVAPI_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/External/")
set(NVAPI_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/External/nvapi/amd64")
set(NVAPI_LIBS "nvapi64")

add_library(nvapi INTERFACE)
target_include_directories(nvapi INTERFACE ${NVAPI_INCLUDE_DIR})
target_link_directories(nvapi INTERFACE ${NVAPI_LIB_DIR})
target_link_libraries(nvapi INTERFACE ${NVAPI_LIBS})

nos_group_targets("nvapi" "External")

set(CMAKE_DEBUG_POSTFIX "")

# nos.sys.vulkan
nos_get_module("nos.sys.vulkan" "5.8" NOS_SYS_VULKAN_TARGET)

# nosDisplay Plugin
# ----------

list(APPEND DEPENDENCIES ${NOS_SYS_VULKAN_TARGET} ${NOS_PLUGIN_SDK_TARGET} nvapi glfw)
list(APPEND INCLUDE_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Source)

nos_add_plugin("nosDisplay" "${DEPENDENCIES}" "${INCLUDE_FOLDERS}")

# Project generation
nos_group_targets("nosDisplay" "NOS Plugins")