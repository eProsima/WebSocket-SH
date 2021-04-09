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

#########################################################################################
# CMake build rules for the Integration Service WebSocket SystemHandle unit tests library
#########################################################################################
find_package(OpenSSL REQUIRED)

include(CTest)
include(${IS_GTEST_CMAKE_MODULE_DIR}/gtest.cmake)
enable_testing()

configure_file(paths.cpp.in ${CMAKE_CURRENT_SOURCE_DIR}/paths.cpp)

add_executable(is-websocket-unit-test
  websocket__jwt.cpp
  paths.cpp
)

target_link_libraries(is-websocket-unit-test
  PRIVATE
    is-websocket
    yaml-cpp
    OpenSSL::SSL
  PUBLIC
    libgtest
)

target_include_directories(is-websocket-unit-test
  PRIVATE
    $<TARGET_PROPERTY:is-websocket,INTERFACE_INCLUDE_DIRECTORIES>
    OpenSSL::SSL
)

add_gtest(is-websocket-unit-test SOURCES websocket__jwt.cpp)
