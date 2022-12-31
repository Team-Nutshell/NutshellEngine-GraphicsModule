set(VMA_LIBRARY_SOURCE_FILES
    VmaUsage.cpp
    VmaUsage.h
    "${PROJECT_SOURCE_DIR}/include/vk_mem_alloc.h"
)

# Visual Studio only debugger symbols
if(WIN32 AND ${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
    set(VMA_LIBRARY_SOURCE_FILES ${VMA_LIBRARY_SOURCE_FILES} vk_mem_alloc.natvis)
endif()

set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_RELWITHDEBINFO_POSTFIX rd)
set(CMAKE_MINSIZEREL_POSTFIX s)

add_library(VulkanMemoryAllocator ${VMA_LIBRARY_SOURCE_FILES})

set_target_properties(
    VulkanMemoryAllocator PROPERTIES

    CXX_EXTENSIONS OFF
    # Use C++14
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(VulkanMemoryAllocator PUBLIC "${PROJECT_SOURCE_DIR}/include")

# Only link to Vulkan if static linking is used
if(${VMA_STATIC_VULKAN_FUNCTIONS})
    target_link_libraries(VulkanMemoryAllocator PUBLIC Vulkan::Vulkan)
endif()

target_compile_definitions(
    VulkanMemoryAllocator

    PUBLIC
    VMA_STATIC_VULKAN_FUNCTIONS=$<BOOL:${VMA_STATIC_VULKAN_FUNCTIONS}>
    VMA_DYNAMIC_VULKAN_FUNCTIONS=$<BOOL:${VMA_DYNAMIC_VULKAN_FUNCTIONS}>
    VMA_DEBUG_ALWAYS_DEDICATED_MEMORY=$<BOOL:${VMA_DEBUG_ALWAYS_DEDICATED_MEMORY}>
    VMA_DEBUG_INITIALIZE_ALLOCATIONS=$<BOOL:${VMA_DEBUG_INITIALIZE_ALLOCATIONS}>
    VMA_DEBUG_GLOBAL_MUTEX=$<BOOL:${VMA_DEBUG_GLOBAL_MUTEX}>
    VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT=$<BOOL:${VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT}>
    VMA_RECORDING_ENABLED=$<BOOL:${VMA_RECORDING_ENABLED}>
)

install(TARGETS VulkanMemoryAllocator DESTINATION "lib")
install(FILES "${PROJECT_SOURCE_DIR}/include/vk_mem_alloc.h" DESTINATION "include")
