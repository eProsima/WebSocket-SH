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
# is-websocket SystemHandle library and tests CMake project

###################################################################################
# CMake build rules for the Integration Service WebSocket SystemHandle library
###################################################################################
cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

project(is-websocket VERSION "3.0.0" LANGUAGES CXX)

###############################################################################
# Load external CMake Modules.
###############################################################################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${SANITIZERS_ROOT}/cmake)

find_package(Sanitizers QUIET)

if(SANITIZE_ADDRESS)
    message(STATUS "Preloading AddressSanitizer library could be done using \"${ASan_WRAPPER}\" to run your program.")
endif()

###################################################################################
# External dependencies for the Integration Service WebSocket SystemHandle library
###################################################################################

find_package(is-core REQUIRED)
find_package(xtypes REQUIRED)
find_package(is-json-xtypes REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(websocketpp REQUIRED)

###################################################################################
# Configure the Integration Service WebSocket SystemHandle library
###################################################################################
add_library(${PROJECT_NAME}
    SHARED
        src/Client.cpp
        src/Endpoint.cpp
        src/JwtValidator.cpp
        src/json_encoding.cpp
        src/Server.cpp
        src/ServerConfig.cpp
        src/ServiceProvider.cpp
        src/TopicPublisher.cpp
    )

if(Sanitizers_FOUND)
    add_sanitizers(${PROJECT_NAME})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION
        ${PROJECT_VERSION}
    SOVERSION
        ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    CXX_STANDARD
        17
    CXX_STANDARD_REQUIRED
        YES
    )

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-pedantic>
        $<$<CXX_COMPILER_ID:GNU>:-fstrict-aliasing>
        $<$<CXX_COMPILER_ID:GNU>:-Wall>
        $<$<CXX_COMPILER_ID:GNU>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU>:-Wcast-align>
        $<$<CXX_COMPILER_ID:GNU>:-Wshadow>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4700>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4820>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4255>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4668>
    )

include(GNUInstallDirs)
message(STATUS "Configuring [${PROJECT_NAME}]...")


target_link_libraries(${PROJECT_NAME}
    PUBLIC
        is::core
    PRIVATE
        is::json-xtypes
        OpenSSL::SSL
    )

target_include_directories(${PROJECT_NAME}
    PRIVATE
        OpenSSL::SSL
    PUBLIC
        ${WEBSOCKETPP_INCLUDE_DIR}
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/cpp-jwt-v1.1.1/include>
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    )


###################################################################################
# Install the Integration Service WebSocket SystemHandle library
###################################################################################
is_install_middleware_plugin(
    MIDDLEWARE
        websocket
    TARGET
        ${PROJECT_NAME}
    TYPES
        websocket_client
        websocket_server
    )

###################################################################################
# Add the Integration Service WebSocket SystemHandle testing subdirectory
###################################################################################
include(CTest)

if(BUILD_TESTS OR BUILD_WEBSOCKET_TESTS)
    add_subdirectory(test)
endif()