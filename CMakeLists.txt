# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

# GLFW
if(NOT TARGET glfw)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/glfw ${CMAKE_CURRENT_BINARY_DIR}/External/glfw EXCLUDE_FROM_ALL)
    nos_get_targets(NOSDISPLAY_GLFW_TARGETS "${CMAKE_CURRENT_BINARY_DIR}/External/glfw")
    nos_group_targets("${NOSDISPLAY_GLFW_TARGETS}" "External")
endif()

set(CMAKE_DEBUG_POSTFIX "")

target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE glfw)
target_compile_definitions(${NOS_PLUGIN_TARGET} PRIVATE NOS_DISABLE_DEPRECATED)

# All Source/*.cpp files are globbed into the plugin target by the build
# system. Exclude the wrong-platform files from compilation by flipping the
# HEADER_FILE_ONLY property so they still show up in IDEs but aren't built.
set(_nos_display_platform_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PlatformWindows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PlatformX11.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/NVIDIACustomResolution.cpp)

if(WIN32)
    set(_nos_display_platform_to_build
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/PlatformWindows.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/NVIDIACustomResolution.cpp)

    # NvAPI: External/nvapi is a submodule with headers and libraries.
    set(NVAPI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/External/")
    set(NVAPI_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/External/nvapi/amd64")
    set(NVAPI_LIBS "nvapi64")

    add_library(nvapi INTERFACE)
    target_include_directories(nvapi INTERFACE ${NVAPI_INCLUDE_DIR})
    target_link_directories(nvapi INTERFACE ${NVAPI_LIB_DIR})
    target_link_libraries(nvapi INTERFACE ${NVAPI_LIBS})
    nos_group_targets("nvapi" "External")

    target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE nvapi)
elseif(APPLE)
    set(_nos_display_platform_to_build "")

    target_sources(${NOS_PLUGIN_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/PlatformCocoa.mm)
    # Compile the .mm file as Objective-C++ without relying on
    # enable_language(OBJCXX) (which is not safe to call from a subdirectory
    # after the root project has been configured).
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Source/PlatformCocoa.mm
        PROPERTIES
            LANGUAGE CXX
            COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
    target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore")
else()
    set(_nos_display_platform_to_build
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/PlatformX11.cpp)
endif()

foreach(_src IN LISTS _nos_display_platform_sources)
    list(FIND _nos_display_platform_to_build "${_src}" _idx)
    if(_idx EQUAL -1)
        # The plugin target was created in a parent directory scope. Use
        # TARGET_DIRECTORY (CMake 3.18+) so the HEADER_FILE_ONLY flag is
        # visible where the target was defined.
        set_source_files_properties("${_src}"
            TARGET_DIRECTORY ${NOS_PLUGIN_TARGET}
            PROPERTIES HEADER_FILE_ONLY TRUE)
    endif()
endforeach()
