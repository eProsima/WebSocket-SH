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

#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/server.hpp>

#include <is/json-xtypes/conversion.hpp>

#include <yaml-cpp/yaml.h>

#include <is/sh/mock/api.hpp>
#include <is/core/Instance.hpp>
#include <is/utils/Convert.hpp>
#include <is/core/runtime/Search.hpp>
#include <is/utils/Log.hpp>

#include <gtest/gtest.h>

#include <iostream>
#include <unordered_set>

using namespace std::chrono_literals;
namespace is = eprosima::is;
namespace xtypes = eprosima::xtypes;

static is::utils::Logger logger("is::sh::WebSocket::test::services");
class ServerTest
{
public:

    typedef websocketpp::server<websocketpp::config::asio> TcpServer;
    typedef websocketpp::server<websocketpp::config::asio_tls> TlsServer;
    typedef websocketpp::endpoint<websocketpp::connection<websocketpp::config::asio_tls>,
                    websocketpp::config::asio_tls> TlsEndpoint;
    typedef websocketpp::endpoint<websocketpp::connection<websocketpp::config::asio>,
                    websocketpp::config::asio> TcpEndpoint;
    typedef boost::asio::ssl::context SslContext;

    ServerTest(
            std::mutex* mutex,
            std::shared_ptr<std::promise<xtypes::DynamicData> > promise,
            const xtypes::DynamicType& req_type,
            const xtypes::DynamicData& response,
            const std::string& yaml_file)
        : _mutex(mutex)
        , _promise(std::move(promise))
        , _req_type(req_type)
        , _response(response)
    {
        YAML::Node configuration = YAML::LoadFile(yaml_file);
        // Gets the port from the websocket client defined on the yaml
        port = configuration["systems"]["ws_client"]["port"].as<int>();

        if (configuration["systems"]["ws_client"]["security"] &&
                configuration["systems"]["ws_client"]["security"].as<std::string>() == "none")
        {
            logger << is::utils::Logger::Level::INFO
                   << "Security Disabled -> Using TCP" << std::endl;

            security = false;
            _thread = new std::thread(std::bind(&ServerTest::configure_tcp_server, this));
        }
        else
        {
            logger << is::utils::Logger::Level::INFO
                   << "Security Enabled -> Using TLS" << std::endl;

            security = true;
            _thread = new std::thread(std::bind(&ServerTest::configure_tls_server, this));
        }
    }

    void configure_tls_server()
    {
        _context = std::make_shared<SslContext>(SslContext::tls);

        boost::system::error_code ec;

        // Looks for the security certificate and key
        is::core::Search ca_search = is::core::Search("websocket")
                .relative_to_config()
                .relative_to_home();

        const std::string cert_file = "certs/websocket_test.crt";
        const std::string cert_file_path = ca_search.find_file(cert_file);

        const std::string key_file = "certs/websocket_test.key";
        const std::string key_file_path = ca_search.find_file(key_file);

        _context->use_certificate_file(cert_file_path, SslContext::pem, ec);
        if (ec)
        {
            logger << is::utils::Logger::Level::ERROR
                   << "Failed to load certificate file '" << cert_file << "': " << ec.message() << std::endl;

            return;
        }
        else
        {
            logger << is::utils::Logger::Level::INFO
                   << "Loaded certificate file '" << cert_file << "'" << std::endl;
        }

        _context->use_rsa_private_key_file(key_file_path, SslContext::pem, ec);
        if (ec)
        {
            logger << is::utils::Logger::Level::ERROR
                   << "Failed to load private key file '" << key_file << "': " << ec.message() << std::endl;
            return;
        }
        else
        {
            logger << is::utils::Logger::Level::INFO
                   << "Loaded certificate file '" << cert_file << "'" << std::endl;
        }

        // Initialize server and define its callbacks
        _tls_server = std::make_shared<TlsServer>(TlsServer());
        _tls_server->set_reuse_addr(true);
        _tls_server->set_access_channels(websocketpp::log::alevel::all);
        _tls_server->clear_access_channels(
            websocketpp::log::alevel::frame_header |
            websocketpp::log::alevel::frame_payload);
        _tls_server->init_asio();
        _tls_server->set_message_handler(
            [&](websocketpp::connection_hdl hdl,
            TlsServer::message_ptr msg)
            {
                on_tls_message(hdl, msg);
            });
        _tls_server->set_tls_init_handler(
            [&](websocketpp::connection_hdl hdl) ->  std::shared_ptr<SslContext>
            {
                return _context;
            });
        _tls_server->set_open_handler(
            [&](
                websocketpp::connection_hdl handle)
            {
                // When the connection is established, sends the advertise_service message so that the client knows that this server
                // manages 'client_request' requests
                const std::string advertise_msg = "{\"op\":\"advertise_service\",\"request_type\":\"Data_Request\",\"reply_type\":\"Data_Response\",\"service\":\"client_request\"}";
                TlsEndpoint::connection_ptr connection = _tls_server->get_con_from_hdl(handle);
                _mutex->lock();
                _open_tls_connections.insert(connection);
                connection->send(advertise_msg);
                _mutex->unlock();
            });
        _tls_server->set_close_handler(
            [&](websocketpp::connection_hdl handle)
            {
                _mutex->lock();
                const auto connection = _tls_server->get_con_from_hdl(handle);
                _open_tls_connections.erase(connection);
                _mutex->unlock();
            });
        logger << is::utils::Logger::Level::INFO
               << "Listening to port: " << port << std::endl;

        _tls_server->listen(port);
        _tls_server->start_accept();
        _tls_server->run();
    }

    void configure_tcp_server()
    {
        _context = std::make_shared<SslContext>(SslContext::tls);
        _context->set_options(
            SslContext::default_workarounds |
            SslContext::no_sslv2 |
            SslContext::no_sslv3);

        // Initialize server and define its callbacks
        _tcp_server = std::make_shared<TcpServer>(TcpServer());
        _tcp_server->set_reuse_addr(true);
        _tcp_server->set_access_channels(websocketpp::log::alevel::all);
        _tcp_server->clear_access_channels(
            websocketpp::log::alevel::frame_header |
            websocketpp::log::alevel::frame_payload);
        _tcp_server->init_asio();
        _tcp_server->set_message_handler(
            [&](websocketpp::connection_hdl hdl,
            TcpServer::message_ptr msg)
            {
                on_tcp_message(hdl, msg);
            });
        _tcp_server->set_tcp_init_handler(
            [&](websocketpp::connection_hdl hdl) ->  std::shared_ptr<SslContext>
            {
                return _context;
            });
        _tcp_server->set_open_handler(
            [&](
                websocketpp::connection_hdl handle)
            {
                // When the connection is established, sends the advertise_service message so that the client knows that this server
                // manages 'client_request' requests
                std::string advertise_msg = "{\"op\":\"advertise_service\",\"request_type\":\"Data_Request\",\"reply_type\":\"Data_Response\",\"service\":\"client_request\"}";
                TcpEndpoint::connection_ptr connection = _tcp_server->get_con_from_hdl(handle);
                _mutex->lock();
                _open_tcp_connections.insert(connection);
                connection->send(advertise_msg);
                _mutex->unlock();
            });
        _tcp_server->set_close_handler(
            [&](websocketpp::connection_hdl handle)
            {
                _mutex->lock();
                const auto connection = _tcp_server->get_con_from_hdl(handle);
                _open_tcp_connections.erase(connection);
                _mutex->unlock();
            });

        logger << is::utils::Logger::Level::INFO
               << "Listening to port: " << port << std::endl;

        _tcp_server->listen(port);
        _tcp_server->start_accept();
        _tcp_server->run();
    }

    // Manages the messages received on the TLS Server
    void on_tls_message(
            websocketpp::connection_hdl hdl,
            TlsServer::message_ptr msg)
    {
        // Sets the server_promise value to notify that the request has arrived correctly
        const auto json_msg = nlohmann::json::parse(msg->get_payload());
        logger << is::utils::Logger::Level::INFO
               << "Msg Received: '" << msg->get_payload() << "'" << std::endl;

        const auto req_it = json_msg.find("args");
        if (req_it != json_msg.end())
        {
            auto d_data = is::json_xtypes::convert(_req_type, req_it.value());
            _promise->set_value(d_data);

            // Sends the response
            auto connection = _tls_server->get_con_from_hdl(hdl);
            const std::string response_msg =
                    "{\"op\":\"service_response\",\"type\":\"Data_Response\",\"service\":\"client_request\",\"id\":\"1\", \"values\":"
                    + is::json_xtypes::convert(_response).dump() + ", \"result\":\"true\"}";

            logger << is::utils::Logger::Level::INFO
                   << "Sending Response: '" << response_msg << "'" << std::endl;

            connection->send(response_msg);
        }
    }

    void on_tcp_message(
            websocketpp::connection_hdl hdl,
            TcpServer::message_ptr msg)
    {
        // Sets the server_promise value to notify that the request has arrived correctly
        const auto json_msg = nlohmann::json::parse(msg->get_payload());
        logger << is::utils::Logger::Level::INFO
               << "Msg Received: '" << msg->get_payload() << "'" << std::endl;

        const auto req_it = json_msg.find("args");
        if (req_it != json_msg.end())
        {
            auto d_data = is::json_xtypes::convert(_req_type, req_it.value());
            _promise->set_value(d_data);

            // Sends the response
            auto connection = _tcp_server->get_con_from_hdl(hdl);
            const std::string response_msg =
                    "{\"op\":\"service_response\",\"type\":\"Data_Response\",\"service\":\"client_request\",\"id\":\"1\", \"values\":"
                    + is::json_xtypes::convert(_response).dump() + ", \"result\":\"true\"}";

            logger << is::utils::Logger::Level::INFO
                   << "Sending Response: '" << response_msg << "'" << std::endl;

            connection->send(response_msg);
        }
    }

    ~ServerTest()
    {
        if (security)
        {
            _mutex->lock();
            // Close all the connections before stopping the server
            for (auto connection : _open_tls_connections)
            {
                if (connection->get_state() != websocketpp::session::state::closed)
                {
                    logger << is::utils::Logger::Level::INFO
                           << "Closing an unclosed connection: " << connection << std::endl;
                    try
                    {
                        connection->close(websocketpp::close::status::normal, "shutdown");
                    }
                    catch (websocketpp::exception e)
                    {
                        logger << is::utils::Logger::Level::WARN
                               << "Exception occurred while trying to close connection " << connection
                               << ". Connection status: "
                               << ((connection->get_state() == websocketpp::session::state::closed) ? "" : "not ")
                               << "closed" << std::endl;
                    }
                }
            }
            _mutex->unlock();
            _tls_server->stop();
        }
        else
        {
            _mutex->lock();
            // Close all the connections before stopping the server
            for (auto connection : _open_tcp_connections)
            {
                if (connection->get_state() != websocketpp::session::state::closed)
                {
                    logger << is::utils::Logger::Level::INFO
                           << "Closing an unclosed connection: " << connection << std::endl;
                    try
                    {
                        connection->close(websocketpp::close::status::normal, "shutdown");
                    }
                    catch (websocketpp::exception e)
                    {
                        logger << is::utils::Logger::Level::WARN
                               << "Exception occurred while trying to close connection " << connection
                               << ". Connection status: "
                               << ((connection->get_state() == websocketpp::session::state::closed) ? "" : "not ")
                               << "closed" << std::endl;
                    }
                }
            }
            _mutex->unlock();
            _tcp_server->stop();
        }
        _thread->join();
    }

private:

    std::shared_ptr<TlsServer> _tls_server;
    std::shared_ptr<TcpServer> _tcp_server;
    std::shared_ptr<SslContext> _context;
    std::unordered_set<TlsEndpoint::connection_ptr> _open_tls_connections;
    std::unordered_set<TcpEndpoint::connection_ptr> _open_tcp_connections;
    std::thread* _thread;
    std::mutex* _mutex;
    const xtypes::DynamicType& _req_type;
    const xtypes::DynamicData& _response;
    std::shared_ptr<std::promise<xtypes::DynamicData> > _promise;
    bool security;
    int port;

};

void test(
        const std::string& yaml_file)
{
    // Creates the Mock and Websocket Integration Service internal entities needed for the test
    is::core::InstanceHandle handle = is::run_instance(yaml_file);
    ASSERT_TRUE(handle);

    // The mock middleware will register both types (request and response) as
    // the same system is used for both routes, while the websocket one needs
    // two different systems (client and server), so each one only registers
    // the type corresponding with the route in which it is involved

    // Get request type from mock middleware
    const is::TypeRegistry& mock_types = *handle.type_registry("mock");
    const xtypes::DynamicType& request_type = *mock_types.at("Data_Request");
    const xtypes::DynamicType& response_type = *mock_types.at("Data_Response");

    // Create the client request message
    xtypes::DynamicData request_msg(request_type);
    request_msg["request"] = "Client Request";

    // Create the server response message
    xtypes::DynamicData response_msg(response_type);
    response_msg["response"] = "Server Response";

    // Create the Websocket Server that will manage the request and response to it
    ServerTest* server;
    std::mutex mutex;
    std::shared_ptr<std::promise<xtypes::DynamicData> > server_promise =
            std::make_shared<std::promise<xtypes::DynamicData> >();
    auto server_future = server_promise->get_future();

    std::thread server_thread([&]()
            {
                server = new ServerTest(&mutex, server_promise, request_type, response_msg, yaml_file);
            });

    // Wait to ensure that there is enough time for client and server matching
    std::this_thread::sleep_for(5s);

    // Send the request using the Mock Client created in the Integration Service
    std::shared_future<xtypes::DynamicData> response_future =
            is::sh::mock::request("client_request", request_msg);

    // First Route: Test Mock Request -> Integration Service Mock Client ->
    // Integration Service Websocket Client (server) -> Test Websocket Server
    {
        // Asures that the request is correctly received on the websocket server
        ASSERT_EQ(std::future_status::ready, server_future.wait_for(2s));
        auto request = std::move(server_future.get());

        logger << is::utils::Logger::Level::INFO
               << "Service request: '" << request << "'" << std::endl;

        EXPECT_EQ(request, request_msg);
    }

    // Second Route: Test Websocket Server -> Integration Service Websocket Client (server) -> Integration Service Mock Client
    {
        ASSERT_EQ(std::future_status::ready, response_future.wait_for(2s));
        xtypes::DynamicData response = std::move(response_future.get());

        logger << is::utils::Logger::Level::INFO
               << "Service response: '" << response << "'" << std::endl;

        EXPECT_EQ(response, response_msg);
    }

    {
        // The illness: DynamicData stores a 'const DynamicType&' instead of a shared_ptr<DynamicType>.
        // The cause: The mock Implementation is static, having a map with clients which stores the promises.
        //            A shared_future doesn't release the data until the promise is destroyed, hence,
        //            as it is stored in a static instance, it is released AFTER the Integration Service instance destruction
        //            which stored the DynamicType.
        // The solution: Use shared_ptr<DynamicType> in DynamicData.
        using namespace std;
        (&response_future)->~shared_future<xtypes::DynamicData>();
    }

    // Quit and wait for no more than a minute. We don't want the test to get
    // hung here indefinitely in the case of an error.
    handle.quit().wait_for(1min);

    // Require that it's no longer running. If it is still running, then it is
    // probably stuck, and we should forcefully quit.
    ASSERT_TRUE(!handle.running());
    ASSERT_EQ(handle.wait(), 0);

    server_thread.join();
}

TEST(WebSocket, Testing_Services_Security)
{
    test(WEBSOCKET__SERVICES__SECURITY__TEST_CONFIG);
}


TEST(WebSocket, Testing_Services_NoSecurity)
{
    test(WEBSOCKET__SERVICES__NOSECURITY__TEST_CONFIG);
}


int main(
        int argc,
        char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
