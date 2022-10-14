cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
set(NAME NutshellGraphicsModule)
project(${NAME} VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NTSH_DEFINES "")

add_subdirectory(external/Module)
add_subdirectory(external/Common)

if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
	find_package(Vulkan)
endif()

if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library. Please install Vulkan SDK or check your install")
endif()

list(APPEND NTSH_DEFINES NTSH_CURRENT_MODULE_GRAPHICS)

set(MODULE_SOURCES
	src/ntsh_graphics_module.cpp)	
set(MODULE_HEADERS
	src/ntsh_graphics_module.h)

set(SOURCES ${MODULE_SOURCES})
set(HEADERS ${MODULE_HEADERS})

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE NutshellEngine::Module NutshellEngine::Common Vulkan::Vulkan)

target_compile_options(${PROJECT_NAME} PRIVATE
	$<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
	$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Werror>)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	${NTSH_DEFINES}
	PUBLIC
	$<$<CONFIG:Debug>:NTSH_DEBUG>
	$<$<CONFIG:MinSizeRel>:NTSH_RELEASE>
	$<$<CONFIG:Release>:NTSH_RELEASE>
	$<$<CONFIG:RelWithDebInfo>:NTSH_DEBUG>)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    DEFINE_SYMBOL "NutshellModule_EXPORTS")