cmake_minimum_required(VERSION 3.15...3.26)

project(VMA LANGUAGES CXX)

add_library(VulkanMemoryAllocator INTERFACE)
add_library(GPUOpen::VulkanMemoryAllocator ALIAS VulkanMemoryAllocator)

target_include_directories(VulkanMemoryAllocator INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

if (CMAKE_VERSION VERSION_LESS "3.21")
    # https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html
    string(COMPARE EQUAL ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR} PROJECT_IS_TOP_LEVEL)
endif()

if (PROJECT_IS_TOP_LEVEL)
    include(GNUInstallDirs)

    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS VulkanMemoryAllocator EXPORT VulkanMemoryAllocatorConfig INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT VulkanMemoryAllocatorConfig NAMESPACE "GPUOpen::" DESTINATION "share/cmake/VulkanMemoryAllocator")
endif()
