/*
 * Copyright (C) 2019 Open Source Robotics Foundation
 * Copyright (C) 2020 - present Proyectos y Sistemas de Mantenimiento SL (eProsima).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <is/sh/mock/api.hpp>
#include <is/core/Instance.hpp>
#include <is/utils/Convert.hpp>
#include <is/utils/Log.hpp>

#include <gtest/gtest.h>

#include <iostream>

namespace is = eprosima::is;
namespace xtypes = eprosima::xtypes;

static is::utils::Logger logger("is::sh::WebSocket::test::roundtrip");

TEST(WebSocket, Transmit_and_receive_all_test_messages)
{
    using namespace std::chrono_literals;

    is::core::InstanceHandle server_handle =
            is::run_instance(WEBSOCKET__ROUNDTRIP_SERVER__TEST_CONFIG);
    ASSERT_TRUE(server_handle);

    is::core::InstanceHandle client_handle =
            is::run_instance(WEBSOCKET__ROUNDTRIP_CLIENT__TEST_CONFIG);
    ASSERT_TRUE(client_handle);

    logger << is::utils::Logger::Level::INFO
           << "Waiting to make sure the client has time to connect" << std::endl;

    std::this_thread::sleep_for(5s);

    logger << is::utils::Logger::Level::INFO
           << "Done waiting!" << std::endl;

    std::promise<xtypes::DynamicData> client_to_server_promise;
    // Note: The public API of is::sh::mock can only publish/subscribe into the
    // Integration Service. An is::sh::mock subscription will never receive a
    // is::sh::mock publication from is::sh::mock::publish_message(~), so any messages
    // that this subscription receives will have come from the server_handle.
    ASSERT_TRUE(is::sh::mock::subscribe(
                "client_to_server",
                [&](const xtypes::DynamicData& message)
                {
                    client_to_server_promise.set_value(message);
                }));

    const is::TypeRegistry& mock_types = *client_handle.type_registry("mock");
    xtypes::DynamicData msg_to_server(*mock_types.at("ClientToServer"));

    const float apple = 2.3f;
    msg_to_server["apple"] = apple;
    is::sh::mock::publish_message("client_to_server", msg_to_server);

    auto client_to_server_future = client_to_server_promise.get_future();
    ASSERT_EQ(client_to_server_future.wait_for(5s), std::future_status::ready);
    const xtypes::DynamicData client_to_server_result = client_to_server_future.get();
    ASSERT_GT(client_to_server_result.size(), 0);

    float apple_result = client_to_server_result["apple"];
    EXPECT_EQ(apple_result, apple);


    std::promise<xtypes::DynamicData> server_to_client_promise;
    ASSERT_TRUE(is::sh::mock::subscribe(
                "server_to_client",
                [&](const xtypes::DynamicData& message)
                {
                    server_to_client_promise.set_value(message);
                }));

    const is::TypeRegistry& server_types = *server_handle.type_registry("mock");
    xtypes::DynamicData msg_to_client(*mock_types.at("ServerToClient"));

    const std::string banana = "here is a banana";
    msg_to_client["banana"] = banana;
    is::sh::mock::publish_message("server_to_client", msg_to_client);

    auto server_to_client_future = server_to_client_promise.get_future();
    ASSERT_EQ(server_to_client_future.wait_for(5s), std::future_status::ready);
    const xtypes::DynamicData server_to_client_result = server_to_client_future.get();
    ASSERT_GT(server_to_client_result.size(), 0);

    std::string banana_result;
    banana_result = server_to_client_result["banana"].value<std::string>();
    EXPECT_EQ(banana_result, banana);

    EXPECT_EQ(client_handle.quit().wait(), 0);
    EXPECT_EQ(server_handle.quit().wait(), 0);

    // NOTE(MXG) It seems the error
    // `[info] asio async_shutdown error: asio.misc:2 (End of file)`
    // is normal and to be expected as far as I can tell.
}

int main(
        int argc,
        char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
