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

#include "Endpoint.hpp"
#include "Errors.hpp"
#include "ServerConfig.hpp"
#include "websocket_types.hpp"
#include "JwtValidator.hpp"

#include <is/core/runtime/Search.hpp>
#include <websocketpp/endpoint.hpp>
#include <websocketpp/http/constants.hpp>

namespace eprosima {
namespace is {
namespace sh {
namespace websocket {

const std::string WebsocketMiddlewareName = "websocket";
const std::string HomeEnvVar = "HOME";
const std::string YamlCertificateKey = "cert";
const std::string YamlPrivateKeyKey = "key";

const std::string YamlFormatKey = "format";
const std::string YamlFormatPemValue = "pem";
const std::string YamlFormatAsn1Value = "asn.1";

const std::string YamlAuthKey = "authentication";

//==============================================================================
static std::string find_websocket_config_file(
        const YAML::Node& configuration,
        const std::string& config_key,
        const std::string& explanation)
{
    utils::Logger logger("is::sh::WebSocket::Server");

    const is::core::Search search = is::core::Search(WebsocketMiddlewareName)
            .relative_to_config()
            .relative_to_home();

    const YAML::Node node = configuration[config_key];
    if (!node)
    {
        logger << utils::Logger::Level::ERROR
               << "'websocket_server' is missing a value for the required parameter '"
               << config_key << "', " << explanation << std::endl;

        return {};
    }

    std::vector<std::string> checked_paths;

    const std::string& parameter = node.as<std::string>();
    const std::string& result = search.find_file(parameter, "", &checked_paths);
    if (result.empty())
    {
        logger << utils::Logger::Level::ERROR
               << "'websocket_server' failed to find the specified file for the '"
               << config_key << "' parameter: '" << parameter << "'. Checked the following paths:\n";
        for (const std::string& checked_path : checked_paths)
        {
            logger << " -- " << checked_path << "\n";
        }
        logger << std::endl;
    }
    else
    {
        logger << utils::Logger::Level::INFO
               << "Using '" << result << "' for '" << config_key << "'" << std::endl;
    }

    return result;
}

//==============================================================================
static std::string find_certificate(
        const YAML::Node& configuration)
{
    return find_websocket_config_file(
        configuration, YamlCertificateKey,
        "which should point to a TLS server certificate!");
}

//==============================================================================
static std::string find_private_key(
        const YAML::Node& configuration)
{
    return find_websocket_config_file(
        configuration, YamlPrivateKeyKey,
        "which should point to the private key for this server!");
}

//==============================================================================
static boost::asio::ssl::context::file_format parse_format(
        const YAML::Node& configuration)
{
    const YAML::Node format = configuration[YamlFormatKey];
    if (!format)
    {
        return boost::asio::ssl::context::pem;
    }

    const std::string& value = format.as<std::string>();
    if (value == YamlFormatPemValue)
    {
        return boost::asio::ssl::context::pem;
    }

    if (value == YamlFormatAsn1Value)
    {
        return boost::asio::ssl::context::asn1;
    }

    throw std::runtime_error(
              "[is::sh::WebSocket::Server] Unrecognized file format type: " + value
              + ". Only [" + YamlFormatPemValue + "] and [" + YamlFormatAsn1Value
              + "] formats are supported.");
}

//==============================================================================
static bool all_tls_closed(
        const std::unordered_set<TlsConnectionPtr>& connections)
{
    for (const auto& connection : connections)
    {
        if (connection->get_state() != websocketpp::session::state::closed)
        {
            return false;
        }
    }

    return true;
}

static bool all_tcp_closed(
        const std::unordered_set<TcpConnectionPtr>& connections)
{
    for (const auto& connection : connections)
    {
        if (connection->get_state() != websocketpp::session::state::closed)
        {
            return false;
        }
    }

    return true;
}

//==============================================================================
class Server : public Endpoint
{
public:

    Server()
        : Endpoint("is::sh::WebSocket::Server")
        , _opened_tls_conn_counter(0)
        , _opened_tcp_conn_counter(0)
    {
        // Do nothing
    }

    TlsEndpoint* configure_tls_endpoint(
            const core::RequiredTypes& /*types*/,
            const YAML::Node& configuration) override
    {
        _use_security = true;
        _tls_server = std::make_shared<TlsServer>(TlsServer());
        const int32_t port = parse_port(configuration);
        if (port < 0)
        {
            return nullptr;
        }
        const uint16_t uport = static_cast<uint16_t>(port);

        const std::string cert_file = find_certificate(configuration);
        if (cert_file.empty())
        {
            _logger << utils::Logger::Level::ERROR
                    << "You must specify a certificate file in your "
                    << "'websocket_server' TLS server configuration!" << std::endl;

            return nullptr;
        }
        else
        {
            _logger << utils::Logger::Level::DEBUG
                    << "Found certificate file: '" << cert_file << "'" << std::endl;
        }

        const std::string key_file = find_private_key(configuration);
        if (key_file.empty())
        {
            _logger << utils::Logger::Level::ERROR
                    << "You must specify a private key in your "
                    << "'websocket_server' TLS server configuration!" << std::endl;

            return nullptr;
        }
        else
        {
            _logger << utils::Logger::Level::DEBUG
                    << "TLS Server: found private key file: '" << key_file << "'" << std::endl;
        }

        const boost::asio::ssl::context::file_format format =
                parse_format(configuration);

        const YAML::Node auth_node = configuration[YamlAuthKey];
        if (auth_node)
        {
            _jwt_validator = std::make_unique<JwtValidator>();
            bool success = ServerConfig::load_auth_policy(*_jwt_validator, auth_node);
            if (!success)
            {
                _logger << utils::Logger::Level::ERROR
                        << "TLS server: error loading auth policy: " << auth_node << std::endl;

                return nullptr;
            }
            else
            {
                _logger << utils::Logger::Level::DEBUG
                        << "TLS server: loaded auth policy: " << auth_node << std::endl;
            }
        }

        if (!configure_server(uport, cert_file, key_file, format))
        {
            return nullptr;
        }

        return _tls_server.get();
    }

    TcpEndpoint* configure_tcp_endpoint(
            const core::RequiredTypes& /*types*/,
            const YAML::Node& configuration) override
    {
        _use_security = false;
        _tcp_server = std::make_shared<TcpServer>(TcpServer());
        const int32_t port = parse_port(configuration);
        if (port < 0)
        {
            return nullptr;
        }
        const uint16_t uport = static_cast<uint16_t>(port);

        const boost::asio::ssl::context::file_format format =
                parse_format(configuration);

        const YAML::Node auth_node = configuration[YamlAuthKey];
        if (auth_node)
        {
            _jwt_validator = std::make_unique<JwtValidator>();
            bool success = ServerConfig::load_auth_policy(*_jwt_validator, auth_node);
            if (!success)
            {
                _logger << utils::Logger::Level::ERROR
                        << "TCP server: error loading auth policy: " << auth_node << std::endl;

                return nullptr;
            }
            else
            {
                _logger << utils::Logger::Level::DEBUG
                        << "TCP server: loaded auth policy: " << auth_node << std::endl;
            }
        }

        if (!configure_server(uport, "", "", format))
        {
            return nullptr;
        }

        return _tcp_server.get();
    }

    bool configure_server(
            const uint16_t port,
            const std::string& cert_file,
            const std::string& key_file,
            const boost::asio::ssl::context::file_format format)
    {
        namespace asio = boost::asio;

        _context = std::make_shared<SslContext>(asio::ssl::context::tls);
        _context->set_options(
            asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 |
            asio::ssl::context::no_sslv3);

        boost::system::error_code ec;
        if (!cert_file.empty())
        {
            _context->use_certificate_file(cert_file, format, ec);
            if (ec)
            {
                _logger << utils::Logger::Level::ERROR
                        << "Failed to load certificate file '"
                        << cert_file << "': " << ec.message() << std::endl;

                return false;
            }
            else
            {
                _logger << utils::Logger::Level::DEBUG
                        << "Loaded certificate file '" << cert_file << "'" << std::endl;
            }
        }

        // TODO(MXG): There is an alternative function
        // _context->use_private_key(key_file, format, ec);
        // which I guess is supposed to be used for keys that do not label
        // themselves as rsa private keys? We're currently using rsa private
        // keys, but this is probably something we should allow users to
        // configure from the Integration Service config file.
        if (!key_file.empty())
        {
            _context->use_rsa_private_key_file(key_file, format, ec);
            if (ec)
            {
                _logger << utils::Logger::Level::ERROR
                        << "Failed to load private key file '" << key_file
                        << "': " << ec.message() << std::endl;

                return false;
            }
            else
            {
                _logger << utils::Logger::Level::DEBUG
                        << "Loaded private key file: '" << key_file << "'" << std::endl;
            }
        }
        // TODO(MXG): This helps to rerun Integration Service more quickly if the server fell down
        // gracelessly. Is this something we really want? Are there any dangers to
        // using this?
        if (_use_security)
        {
            initialize_tls_server(port);
        }
        else
        {
            initialize_tcp_server(port);
        }

        return true;
    }

    void initialize_tls_server(
            uint16_t port)
    {
        _logger << utils::Logger::Level::INFO
                << "Initializing TLS server on port " << port << std::endl;

        _tls_server->set_reuse_addr(true);

        _tls_server->clear_access_channels(
            websocketpp::log::alevel::frame_header |
            websocketpp::log::alevel::frame_payload);

        _tls_server->init_asio();
        _tls_server->start_perpetual();

        _tls_server->set_message_handler(
            [&](ConnectionHandlePtr handle, TlsMessagePtr message)
            {
                this->_handle_tls_message(handle, message);
            });

        _tls_server->set_close_handler(
            [&](ConnectionHandlePtr handle)
            {
                this->_handle_close(std::move(handle));
            });

        _tls_server->set_open_handler(
            [&](ConnectionHandlePtr handle)
            {
                this->_handle_opening(std::move(handle));
            });

        _tls_server->set_fail_handler(
            [&](ConnectionHandlePtr handle)
            {
                this->_handle_failed_connection(std::move(handle));
            });

        _tls_server->set_tls_init_handler(
            [&](ConnectionHandlePtr /*handle*/) -> SslContextPtr
            {
                return _context;
            });

        _tls_server->set_validate_handler(
            [&](ConnectionHandlePtr handle) -> bool
            {
                return this->_handle_validate(std::move(handle));
            });

        _tls_server->listen(port);

        _server_thread = std::thread([&]()
                        {
                            this->_tls_server->run();
                        });

    }

    void initialize_tcp_server(
            uint16_t port)
    {
        _logger << utils::Logger::Level::INFO
                << "Initializing TCP server on port " << port << std::endl;

        _tcp_server->set_reuse_addr(true);

        _tcp_server->clear_access_channels(
            websocketpp::log::alevel::frame_header |
            websocketpp::log::alevel::frame_payload);

        _tcp_server->init_asio();
        _tcp_server->start_perpetual();

        _tcp_server->set_message_handler(
            [&](ConnectionHandlePtr handle, TlsMessagePtr message)
            {
                this->_handle_tcp_message(handle, message);
            });

        _tcp_server->set_close_handler(
            [&](ConnectionHandlePtr handle)
            {
                this->_handle_close(std::move(handle));
            });

        _tcp_server->set_open_handler(
            [&](ConnectionHandlePtr handle)
            {
                this->_handle_opening(std::move(handle));
            });

        _tcp_server->set_fail_handler(
            [&](ConnectionHandlePtr handle)
            {
                this->_handle_failed_connection(std::move(handle));
            });

        _tcp_server->set_tcp_init_handler(
            [&](ConnectionHandlePtr /*handle*/) -> SslContextPtr
            {
                return _context;
            });

        _tcp_server->set_validate_handler(
            [&](ConnectionHandlePtr handle) -> bool
            {
                return this->_handle_validate(std::move(handle));
            });

        _tcp_server->listen(port);

        _server_thread = std::thread([&]()
                        {
                            this->_tcp_server->run();
                        });

    }

    ~Server() override
    {
        _closing_down = true;

        // NOTE(MXG): _open_connections can get modified in other threads so we'll
        // make a copy of it here before using it.
        // TODO(MXG): We should probably be using mutexes to protect the operations
        // on these connections.

        // First instruct all connections to close
        if (_use_security)
        {
            _mutex.lock();
            for (const auto& connection : _open_tls_connections)
            {
                if (connection->get_state() != websocketpp::session::state::closed)
                {
                    try
                    {
                        connection->close(websocketpp::close::status::normal, "shutdown");
                    }
                    catch (websocketpp::exception& e)
                    {
                        _logger << utils::Logger::Level::WARN
                                << "Exception ocurred while closing connection";
                        if (_open_tls_conn_to_id.end() != _open_tls_conn_to_id.find(connection))
                        {
                            _logger << " with ID '" << _open_tls_conn_to_id[connection] << "'";
                        }
                        _logger << std::endl;
                    }
                }
            }

            // Then wait for all of them to close

            using namespace std::chrono_literals;
            const auto start_time = std::chrono::steady_clock::now();
            // TODO(MXG): Make these timeout parameters something that can be
            // configured by users.
            while (!all_tls_closed(_open_tls_connections))
            {
                std::this_thread::sleep_for(200ms);

                if (std::chrono::steady_clock::now() - start_time > 10s)
                {
                    _logger << utils::Logger::Level::ERROR
                            << "Timed out while waiting for "
                            << "the remote clients to acknowledge the connection "
                            << "shutdown request" << std::endl;

                    break;
                }
            }
            _mutex.unlock();
        }
        else
        {
            _mutex.lock();
            for (const auto& connection : _open_tcp_connections)
            {
                if (connection->get_state() != websocketpp::session::state::closed)
                {
                    try
                    {
                        connection->close(websocketpp::close::status::normal, "shutdown");
                    }
                    catch (websocketpp::exception& e)
                    {
                        _logger << utils::Logger::Level::WARN
                                << "Exception ocurred while closing connection";
                        if (_open_tcp_conn_to_id.end() != _open_tcp_conn_to_id.find(connection))
                        {
                            _logger << " with ID '" << _open_tcp_conn_to_id[connection] << "'";
                        }
                        _logger << std::endl;
                    }
                }
            }
            // Then wait for all of them to close

            using namespace std::chrono_literals;
            const auto start_time = std::chrono::steady_clock::now();
            // TODO(MXG): Make these timeout parameters something that can be
            // configured by users.
            while (!all_tcp_closed(_open_tcp_connections))
            {
                std::this_thread::sleep_for(200ms);

                if (std::chrono::steady_clock::now() - start_time > 10s)
                {
                    _logger << utils::Logger::Level::ERROR
                            << "Timed out while waiting for "
                            << "the remote clients to acknowledge the connection "
                            << "shutdown request" << std::endl;

                    break;
                }
            }
            _mutex.unlock();
        }

        if (_server_thread.joinable())
        {
            if (_use_security)
            {
                _tls_server->stop();
            }
            else
            {
                _tcp_server->stop();
            }

            _server_thread.join();
        }
    }

    bool okay() const override
    {
        // TODO(MXG): How do we know if the server is okay?
        return true;
    }

    bool spin_once() override
    {
        if (!_has_spun_once)
        {
            _has_spun_once = true;
            if (_use_security)
            {
                _tls_server->start_accept();
            }
            else
            {
                _tcp_server->start_accept();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // TODO(MXG): How do we know if the server is okay?
        return true;
    }

    void runtime_advertisement(
            const std::string& topic,
            const xtypes::DynamicType& message_type,
            const std::string& id,
            const YAML::Node& configuration) override
    {
        const std::string advertise_msg =
                get_encoding().encode_advertise_msg(
            topic, message_type.name(), id, configuration);

        if (_use_security)
        {
            _mutex.lock();
            for (const TlsConnectionPtr& connection : _open_tls_connections)
            {
                connection->send(advertise_msg);
            }
            _mutex.unlock();
        }
        else
        {
            _mutex.lock();
            for (const TcpConnectionPtr& connection : _open_tcp_connections)
            {
                connection->send(advertise_msg);
            }
            _mutex.unlock();
        }
    }

private:

    void _handle_tls_message(
            const ConnectionHandlePtr& handle,
            const TlsMessagePtr& message)
    {
        get_encoding().interpret_websocket_msg(
            message->get_payload(), *this, _tls_server->get_con_from_hdl(handle));
    }

    void _handle_tcp_message(
            const ConnectionHandlePtr& handle,
            const TcpMessagePtr& message)
    {
        get_encoding().interpret_websocket_msg(
            message->get_payload(), *this, _tcp_server->get_con_from_hdl(handle));
    }

    void _handle_close(
            const ConnectionHandlePtr& handle)
    {
        _mutex.lock();
        if (_use_security)
        {
            const auto connection = _tls_server->get_con_from_hdl(handle);
            const uint16_t connection_id = _open_tls_conn_to_id[connection];

            _open_tls_conn_to_id.erase(connection);

            notify_connection_closed(connection);

            _open_tls_connections.erase(connection);

            _logger << utils::Logger::Level::INFO
                    << "Closed TLS client connection with ID '" << connection_id
                    << "'. Now, " << _open_tls_connections.size()
                    << " TLS connections remain active" << std::endl;
        }
        else
        {
            const auto connection = _tcp_server->get_con_from_hdl(handle);
            const uint16_t connection_id = _open_tcp_conn_to_id[connection];

            _open_tcp_conn_to_id.erase(connection);

            notify_connection_closed(connection);

            _open_tcp_connections.erase(connection);

            _logger << utils::Logger::Level::INFO
                    << "Closed TCP client connection with ID '" << connection_id
                    << "'. Now, " << _open_tcp_connections.size()
                    << " TCP connections remain active" << std::endl;
        }
        _mutex.unlock();
    }

    void _handle_opening(
            const ConnectionHandlePtr& handle)
    {
        _mutex.lock();
        if (_use_security)
        {
            const auto connection = _tls_server->get_con_from_hdl(handle);

            if (_closing_down)
            {
                connection->close(websocketpp::close::status::normal, "shutdown");
                return;
            }

            _open_tls_conn_to_id.insert({connection, ++_opened_tls_conn_counter});

            notify_connection_opened(connection);

            _open_tls_connections.insert(connection);

            _logger << utils::Logger::Level::INFO
                    << "Opened TLS connection with ID '" << _opened_tls_conn_counter << "'. "
                    << "Number of active TLS connections: " << _open_tls_connections.size() << std::endl;
        }
        else
        {
            const auto connection = _tcp_server->get_con_from_hdl(handle);

            if (_closing_down)
            {
                connection->close(websocketpp::close::status::normal, "shutdown");
                return;
            }

            _open_tcp_conn_to_id.insert({connection, ++_opened_tcp_conn_counter});

            notify_connection_opened(connection);

            _open_tcp_connections.insert(connection);

            _logger << utils::Logger::Level::INFO
                    << "Opened TCP connection with ID '" << _opened_tcp_conn_counter << "'. "
                    << "Number of active TCP connections: " << _open_tcp_connections.size() << std::endl;
        }
        _mutex.unlock();
    }

    void _handle_failed_connection(
            const ConnectionHandlePtr& /*handle*/)
    {
        _logger << utils::Logger::Level::WARN
                << "An incoming client failed to " << "connect." << std::endl;
    }

    bool _handle_validate(
            const ConnectionHandlePtr& handle)
    {
        if (!_jwt_validator)
        {
            return true;
        }

        if (_use_security)
        {
            TlsConnectionPtr connection_ptr = _tls_server->get_con_from_hdl(handle);
            std::vector<std::string> requested_sub_protos = connection_ptr->get_requested_subprotocols();
            if (requested_sub_protos.size() != 1)
            {
                connection_ptr->set_status(websocketpp::http::status_code::unauthorized);
                return false; // a valid Integration Service client should always send exactly 1 subprotocols.
            }

            const std::string token = requested_sub_protos[0]; // the subprotocol is the jwt token

            try
            {
                _jwt_validator->verify(token);
            }
            catch (const jwt::VerificationError& e)
            {
                _logger << utils::Logger::Level::ERROR
                        << "Error while validating token on TLS server'" << token
                        << "'" << e.what() << std::endl;

                connection_ptr->set_status(websocketpp::http::status_code::unauthorized);
                return false;
            }

            connection_ptr->select_subprotocol(token);
            return true;
        }
        else
        {
            TcpConnectionPtr connection_ptr = _tcp_server->get_con_from_hdl(handle);
            std::vector<std::string> requested_sub_protos = connection_ptr->get_requested_subprotocols();
            if (requested_sub_protos.size() != 1)
            {
                connection_ptr->set_status(websocketpp::http::status_code::unauthorized);
                return false; // a valid Integration Service client should always send exactly 1 subprotocols.
            }

            const std::string token = requested_sub_protos[0]; // the subprotocol is the jwt token

            try
            {
                _jwt_validator->verify(token);
            }
            catch (const jwt::VerificationError& e)
            {
                _logger << utils::Logger::Level::ERROR
                        << "Error while validating token on TCP server'" << token
                        << "'" << e.what() << std::endl;

                connection_ptr->set_status(websocketpp::http::status_code::unauthorized);
                return false;
            }

            connection_ptr->select_subprotocol(token);
            return true;
        }
    }

    std::shared_ptr<TlsServer> _tls_server;
    std::shared_ptr<TcpServer> _tcp_server;
    bool _use_security;
    std::thread _server_thread;
    std::mutex _mutex;
    EncodingPtr _encoding;
    SslContextPtr _context;
    std::unordered_set<TlsConnectionPtr> _open_tls_connections;
    uint16_t _opened_tls_conn_counter;
    std::unordered_map<TlsConnectionPtr, uint16_t> _open_tls_conn_to_id;
    std::unordered_set<TcpConnectionPtr> _open_tcp_connections;
    uint16_t _opened_tcp_conn_counter;
    std::unordered_map<TcpConnectionPtr, uint16_t> _open_tcp_conn_to_id;
    bool _has_spun_once = false;
    bool _closing_down = false;
    std::unique_ptr<JwtValidator> _jwt_validator;

};

IS_REGISTER_SYSTEM("websocket_server", is::sh::websocket::Server)

} //  namespace websocket
} //  namespace sh
} //  namespace is
} //  namespace eprosima
