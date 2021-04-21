# Copyright 2019 Open Source Robotics Foundation, Inc.
# Copyright (C) 2020 - present Proyectos y Sistemas de Mantenimiento SL (eProsima).
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# is-websocket SystemHandle library and integration-service executable CMake project

###############################################################################################
# CMake build rules for the Integration Service WebSocket SystemHandle integration test library
###############################################################################################
cmake_minimum_required(VERSION 3.5.0)

project(is-websocket-test)

find_package(is-websocket REQUIRED)
find_package(is-mock REQUIRED)
find_package(is-json-xtypes REQUIRED)
find_package(xtypes REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(websocketpp REQUIRED)

macro(compile_test)
    # Parse arguments
    if("${ARGV0}" STREQUAL "NAME")
        set(uniValueArgs NAME)
    else()
        set(TEST_NAME "${ARGV0}")
    endif()
    set(multiValueArgs SOURCE)
    cmake_parse_arguments(TEST "" "${uniValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${TEST_NAME} ${TEST_SOURCE})

    target_link_libraries(${TEST_NAME}
        PUBLIC
            is::mock
            is::websocket
            is::json-xtypes
            Threads::Threads
            OpenSSL::SSL
        PRIVATE
	    $<IF:$<BOOL:${IS_GTEST_EXTERNAL_PROJECT}>,libgtest,gtest>
	)

    target_include_directories(${TEST_NAME}
      PRIVATE
          ${WEBSOCKETPP_INCLUDE_DIR}
        )

    target_compile_options(${TEST_NAME}
        INTERFACE
            $<$<CXX_COMPILER_ID:GNU>:-Wall>
            $<$<CXX_COMPILER_ID:GNU>:-Wextra>
            $<$<CXX_COMPILER_ID:GNU>:-pedantic>
        )

    set_target_properties(${TEST_NAME}
        PROPERTIES
            CXX_STANDARD
                17
            CXX_STANDARD_REQUIRED
                YES
        )

    foreach(resource
        websocket__roundtrip_server.yaml
        websocket__roundtrip_client.yaml
        websocket__dispatch__security.yaml
        websocket__dispatch__nosecurity.yaml
        websocket__services__security.yaml
        websocket__services__nosecurity.yaml)

        get_filename_component(resource_name ${resource} NAME_WE)
        string(TOUPPER ${resource_name} CAPS_RESOURCE_NAME)

        target_compile_definitions(${TEST_NAME}
            PRIVATE
                "${CAPS_RESOURCE_NAME}__TEST_CONFIG=\"${CMAKE_CURRENT_LIST_DIR}/resources/${resource}\""
            )

    endforeach()

    add_gtest(${TEST_NAME} SOURCES ${TEST_SOURCE})
endmacro()

include(CTest)
include(${IS_GTEST_CMAKE_MODULE_DIR}/gtest.cmake)
enable_testing()

compile_test(${PROJECT_NAME}_rountrip_test SOURCE integration/websocket__roundtrip.cpp)
compile_test(${PROJECT_NAME}_dispatch_test SOURCE integration/websocket__dispatch.cpp)
compile_test(${PROJECT_NAME}_services_test SOURCE integration/websocket__services.cpp)

# Windows dll dependencies installation
if(WIN32)
    find_file(JSONDLL NAMES "is-json-xtypes.dll" PATHS "${is-json-xtypes_DIR}" PATH_SUFFIXES "lib" )
    install (FILES ${JSONDLL} DESTINATION "${CMAKE_INSTALL_PREFIX}/../bin")
endif()
