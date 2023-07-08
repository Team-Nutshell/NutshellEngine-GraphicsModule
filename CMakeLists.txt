cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
set(NAME NutshellEngine-GraphicsModule)
project(${NAME} VERSION 0.0.1 LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NTSHENGN_DEFINES "")

set(NTSHENGN_COMMON_PATH "NtshEngn_Common_NOTFOUND" CACHE PATH "The directory containing NutshellEngine's common resources.")
if(NOT ${NTSHENGN_COMMON_PATH} STREQUAL "NtshEngn_Common_NOTFOUND" AND EXISTS ${NTSHENGN_COMMON_PATH})
	message(STATUS "Copy NutshellEngine\'s common resources from ${NTSHENGN_COMMON_PATH} to ${CMAKE_CURRENT_SOURCE_DIR}/Common")
	file(COPY ${NTSHENGN_COMMON_PATH}/ DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Common)

	add_subdirectory(Common)
else()
	message(STATUS "Copy NutshellEngine\'s common resources from Git repository to ${CMAKE_CURRENT_SOURCE_DIR}/Common")
	FetchContent_Declare(
		Common
		GIT_REPOSITORY https://github.com/Team-Nutshell/NutshellEngine-Common
		GIT_TAG main
		SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Common"
	)
	
	FetchContent_MakeAvailable(Common)
endif()

set(NTSHENGN_MODULE_PATH "NtshEngn_Module_NOTFOUND" CACHE PATH "The directory containing NutshellEngine's common module resources.")
if(NOT ${NTSHENGN_MODULE_PATH} STREQUAL "NtshEngn_Module_NOTFOUND" AND EXISTS ${NTSHENGN_MODULE_PATH})
	message(STATUS "Copy NutshellEngine\'s common module resources from ${NTSHENGN_MODULE_PATH} to ${CMAKE_CURRENT_SOURCE_DIR}/Module")
	file(COPY ${NTSHENGN_MODULE_PATH}/ DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/Module)

	add_subdirectory(Module)
else()
	message(STATUS "Copy NutshellEngine\'s common module resources from Git repository to ${CMAKE_CURRENT_SOURCE_DIR}/Module")
	FetchContent_Declare(
		Module
		GIT_REPOSITORY https://github.com/Team-Nutshell/NutshellEngine-Module
		GIT_TAG main
		SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Module"
	)

	FetchContent_MakeAvailable(Module)
endif()

list(APPEND NTSHENGN_DEFINES NTSHENGN_CURRENT_MODULE_GRAPHICS)

set(MODULE_SOURCES
	src/ntshengn_graphics_module.cpp)	
set(MODULE_HEADERS
	src/ntshengn_graphics_module.h)

set(EXTERNAL_HEADERS
	external/d3dx12/d3dx12.h)

set(SOURCES ${MODULE_SOURCES})
set(HEADERS ${MODULE_HEADERS} ${EXTERNAL_HEADERS})

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE NutshellEngine::Module NutshellEngine::Common d3d12.lib dxgi.lib d3dcompiler.lib)

target_compile_options(${PROJECT_NAME} PRIVATE
	$<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
	$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Werror>)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	${NTSHENGN_DEFINES}
	PUBLIC
	$<$<CONFIG:Debug>:NTSHENGN_DEBUG>
	$<$<CONFIG:MinSizeRel>:NTSHENGN_RELEASE>
	$<$<CONFIG:Release>:NTSHENGN_RELEASE>
	$<$<CONFIG:RelWithDebInfo>:NTSHENGN_DEBUG>)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    DEFINE_SYMBOL "NutshellEngineModule_EXPORTS")

set(NTSHENGN_INSTALL_MODULE_PATH "NtshEngn_InstallModule_NOTFOUND" CACHE PATH "Give the option to install the built module into NutshellEngine\'s modules directory.")
if(EXISTS ${NTSHENGN_INSTALL_MODULE_PATH})
	message(STATUS "Found NutshellEngine\'s modules directory at ${NTSHENGN_INSTALL_MODULE_PATH}.")
	set(CMAKE_INSTALL_PREFIX ${NTSHENGN_INSTALL_MODULE_PATH})
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		install(TARGETS ${PROJECT_NAME}
			RUNTIME
			DESTINATION ${NTSHENGN_INSTALL_MODULE_PATH})
	else()
		install(TARGETS ${PROJECT_NAME}
			LIBRARY
			DESTINATION ${NTSHENGN_INSTALL_MODULE_PATH})
	endif()
elseif(NOT NTSHENGN_INSTALL_MODULE_PATH STREQUAL "NtshEngn_InstallModule_NOTFOUND")
	message(WARNING "Could not find NutshellEngine\'s modules directory at ${NTSHENGN_INSTALL_MODULE_PATH}.")
endif()