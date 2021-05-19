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

#ifndef _WEBSOCKET_IS_SH__SRC__ENDPOINT_HPP_
#define _WEBSOCKET_IS_SH__SRC__ENDPOINT_HPP_

#include "Encoding.hpp"
#include "websocket_types.hpp"

#include <is/systemhandle/SystemHandle.hpp>
#include <is/utils/Log.hpp>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace xtypes = eprosima::xtypes;

namespace eprosima {
namespace is {
namespace sh {
namespace websocket {

const std::string YamlEncodingKey = "encoding";
const std::string YamlEncoding_Json = "json";
const std::string YamlPortKey = "port";
const std::string YamlHostKey = "host";

/**
 * @class Endpoint
 *        Represents a *WebSocket* endpoint for the *Integration Service*.
          The Endpoint class will be later specialized for client and server applications.
 */
class Endpoint : public is::FullSystem, public ServiceClient
{
public:

    /**
     * @brief Constructor.
     *
     * @param name The name given to this Endpoint instance.
     * It will be used to identify logging traces.
     */
    Endpoint(
            const std::string& name);

    /**
     * @brief Inherited from SystemHandle.
     */
    bool configure(
            const core::RequiredTypes& types,
            const YAML::Node& configuration,
            TypeRegistry& type_registry) override;

    /**
     * @brief Inherited from SystemHandle.
     */
    virtual bool okay() const = 0;

    /**
     * @brief Inherited from SystemHandle.
     */
    virtual bool spin_once() = 0;

    /**
     * @brief Destructor.
     */
    virtual ~Endpoint() = default;


    /**
     * @brief Inherited from TopicSubscriberSystem.
     */
    bool subscribe(
            const std::string& topic_name,
            const xtypes::DynamicType& message_type,
            TopicSubscriberSystem::SubscriptionCallback* callback,
            const YAML::Node& configuration) override final;

    /**
     * @brief Inherited from TopicPublisherSystem.
     */
    std::shared_ptr<TopicPublisher> advertise(
            const std::string& topic_name,
            const xtypes::DynamicType& message_type,
            const YAML::Node& configuration) override final;

    /**
     * @brief Inherited from ServiceClientSystem.
     */
    bool create_client_proxy(
            const std::string& service_name,
            const xtypes::DynamicType& service_type,
            ServiceClientSystem::RequestCallback* callback,
            const YAML::Node& configuration) override final;

    /**
     * @brief Inherited from ServiceClientSystem.
     */
    bool create_client_proxy(
            const std::string& service_name,
            const xtypes::DynamicType& request_type,
            const xtypes::DynamicType& reply_type,
            ServiceClientSystem::RequestCallback* callback,
            const YAML::Node& configuration) override final;

    /**
     * @brief Inherited from ServiceProviderSystem.
     */
    std::shared_ptr<ServiceProvider> create_service_proxy(
            const std::string& service_name,
            const xtypes::DynamicType& service_type,
            const YAML::Node& configuration) override final;

    /**
     * @brief Inherited from ServiceProviderSystem.
     */
    std::shared_ptr<ServiceProvider> create_service_proxy(
            const std::string& service_name,
            const xtypes::DynamicType& request_type,
            const xtypes::DynamicType& reply_type,
            const YAML::Node& configuration) override final;


    /**
     * @brief Send out an advertisement the next time a connection is made.
     *
     * @param[in] topic The topic name.
     *
     * @param[in] message_type The Dynamic Type message representation.
     *
     * @param[in] id The publisher ID.
     *
     * @param[in] configuration Additional configuration, in *YAML* format,
     *            required to advertise the topic.
     */
    void startup_advertisement(
            const std::string& topic,
            const xtypes::DynamicType& message_type,
            const std::string& id,
            const YAML::Node& configuration);

    /**
     * @brief Send out an advertisement to all existing connections right away.
     *        This is for publication topics that are determined at runtime by topic templates.
     *
     * @param[in] topic The topic name.
     *
     * @param[in] message_type The Dynamic Type message representation.
     *
     * @param[in] id The message ID.
     *
     * @param[in] configuration Additional configuration, in *YAML* format,
     *            required to advertise the topic.
     */
    virtual void runtime_advertisement(
            const std::string& topic,
            const xtypes::DynamicType& message_type,
            const std::string& id,
            const YAML::Node& configuration) = 0;

    /**
     * @brief Publish a message to a certain topic.
     *
     * @see is::TopicPublisher.
     *
     * @param[in] topic The topic name to publish to.
     *
     * @param[in] message The message data instance to be published.
     *
     * @returns `true` if the publication was made, `false` otherwise.
     */
    bool publish(
            const std::string& topic,
            const xtypes::DynamicData& message);

    /**
     * @brief Call a service.
     *
     * @see is::ServiceProvider.
     *
     * @param[in] service The name of the service to be called.
     *
     * @param[in] request Request message for the service.
     *
     * @param[in,out] client The proxy for the client that is making the request.
     *
     * @param[in] call_handle A handle for the call.
     */
    void call_service(
            const std::string& service,
            const xtypes::DynamicData& request,
            ServiceClient& client,
            std::shared_ptr<void> call_handle);

    /**
     * @brief Inherited from ServiceClient.
     */
    void receive_response(
            std::shared_ptr<void> call_handle,
            const xtypes::DynamicData& response) override final;

    /**
     * @brief Process an advertisement message.
     *        This step is required prior to publish operation.
     *
     * @param[in] topic_name The name of the topic
     *            to be advertised.
     *
     * @param[in] message_type The type name of the topic
     *            to be advertised.
     *
     * @param[in] id The publisher ID.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_topic_advertisement_ws(
            const std::string& topic_name,
            const xtypes::DynamicType& message_type,
            const std::string& id,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process an unadvertisement message.
     *
     * @param[in] topic_name The name of the topic
     *            to be unadvertised.
     *
     * @param[in] id The publisher ID.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_topic_unadvertisement_ws(
            const std::string& topic_name,
            const std::string& id,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process an publication.
     *
     * @param[in] topic_name The name of the topic
     *            where the message will be published to.
     *
     * @param[in] message The message published.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_publication_ws(
            const std::string& topic_name,
            const xtypes::DynamicData& message,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process a request for subscribing to a certain topic.
     *
     * @param[in] topic_name The name of the topic
     *            where the subscription will be performed to.
     *
     * @param[in] message_type The dynamic type of the topic
     *            to get subscribed to.
     *
     * @param[in] id The identifier of the message.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_subscribe_request_ws(
            const std::string& topic_name,
            const xtypes::DynamicType* message_type,
            const std::string& id,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process a request for unsubscribing to a certain topic.
     *
     * @param[in] topic_name The name of the topic
     *            where the subscription will be stopped.
     *
     * @param[in] id The identifier of the message.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_unsubscribe_request_ws(
            const std::string& topic_name,
            const std::string& id,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process a service request.
     *
     * @param[in] service_name The name of the service.
     *
     * @param[in] request The request data.
     *
     * @param[in] id The service ID.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_service_request_ws(
            const std::string& service_name,
            const xtypes::DynamicData& request,
            const std::string& id,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process a service advertisement. This is required prior to calling a service.
     *
     * @param[in] service_name The name of the service.
     *
     * @param[in] req_type The request data type.
     *
     * @param[in] reply_type The reply data type.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_service_advertisement_ws(
            const std::string& service_name,
            const xtypes::DynamicType& req_type,
            const xtypes::DynamicType& reply_type,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process a service unadvertisement. The service will no longer be available.
     *
     * @param[in] service_name The name of the service.
     *
     * @param[in] service_type The service type. Usually refers to the request type.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_service_unadvertisement_ws(
            const std::string& service_name,
            const xtypes::DynamicType* service_type,
            std::shared_ptr<void> connection_handle);

    /**
     * @brief Process a service response.
     *
     * @param[in] service_name The name of the service.
     *
     * @param[in] response The response data.
     *
     * @param[in] id The service ID.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    void receive_service_response_ws(
            const std::string& service_name,
            const xtypes::DynamicData& response,
            const std::string& id,
            std::shared_ptr<void> connection_handle);

protected:

    /**
     * @brief Getter method for the Encoding.
     *
     * @returns a const reference to the _encoding attribute.
     */
    const Encoding& get_encoding() const;

    /**
     * @brief Notify when a TLS connection has been opened.
     *
     * @param[in] connection_handle The TLS handle used to send
     *            the notification message.
     */
    void notify_connection_opened(
            const TlsConnectionPtr& connection_handle);

    /**
     * @brief Notify when a TCP connection has been opened.
     *
     * @param[in] connection_handle The TCP handle used to send
     *            the notification message.
     */
    void notify_connection_opened(
            const TcpConnectionPtr& connection_handle);

    /**
     * @brief Notify when a connection has been closed.
     *
     * @param[in] connection_handle The handle used to send
     *            the notification message.
     */
    void notify_connection_closed(
            const std::shared_ptr<void>& connection_handle);

    /**
     * @brief Get the *WebSocket* port, as specified in the configuration file.
     *        This method will warn to the user if no port is present.
     *
     * @param[in] configuration The configuration relative to the *WebSocket* Endpoint,
     *            as specified in the *YAML* configuration file.
     *
     * @returns The port number, or -1 if invalid.
     */
    int32_t parse_port(
            const YAML::Node& configuration);

    utils::Logger _logger;

private:

    /**
     * @brief Configure the TLS Endpoint.
     *        This method shall be overriden for each Endpoint implementation,
     *        namely, Client and Server.
     *
     * @param[in] types Set of types that must be present in the Integration Service instance.
     *
     * @param[in] configuration Specific configuration to be applied to this *WebSocket*
     *            Endpoint, as specified in the *YAML* file.
     *
     * @returns A pointer to the configured TLS Endpoint.
     */
    virtual TlsEndpoint* configure_tls_endpoint(
            const core::RequiredTypes& types,
            const YAML::Node& configuration) = 0;

    /**
     * @brief Configure the TCP Endpoint.
     *        This method shall be overriden for each Endpoint implementation,
     *        namely, Client and Server.
     *
     * @param[in] types Set of types that must be present in the Integration Service instance.
     *
     * @param[in] configuration Specific configuration to be applied to this *WebSocket*
     *            Endpoint, as specified in the *YAML* file.
     *
     * @returns A pointer to the configured TCP Endpoint.
     */
    virtual TcpEndpoint* configure_tcp_endpoint(
            const core::RequiredTypes& types,
            const YAML::Node& configuration) = 0;

    /**
     * Class members.
     */
    EncodingPtr _encoding;
    std::shared_ptr<TlsEndpoint> _tls_endpoint;
    std::shared_ptr<TcpEndpoint> _tcp_endpoint;
    bool _use_security;

    struct TopicSubscribeInfo
    {
        std::string type;
        SubscriptionCallback* callback;

        /**
         * Connections whose publications we will ignore because
         * their message type does not match the one we expect.
         */
        std::unordered_set<std::shared_ptr<void> > blacklist;
    };

    struct TopicPublishInfo
    {
        std::string type;

        using ListenerMap = std::unordered_map<
            std::shared_ptr<void>,
            std::unordered_set<std::string> >;

        /**
         * Map from connection handle to listeners ID.
         */
        ListenerMap listeners;
    };

    struct ClientProxyInfo
    {
        std::string req_type;
        std::string reply_type;
        RequestCallback* callback;
        YAML::Node configuration;
    };

    struct ServiceProviderInfo
    {
        std::string req_type;
        std::string reply_type;
        std::shared_ptr<void> connection_handle;
        YAML::Node configuration;
    };

    struct ServiceRequestInfo
    {
        ServiceClient* client;
        std::shared_ptr<void> call_handle;
    };

    std::vector<std::string> _startup_messages;
    std::unordered_map<std::string, TopicSubscribeInfo> _topic_subscribe_info;
    std::unordered_map<std::string, TopicPublishInfo> _topic_publish_info;
    std::unordered_map<std::string, ClientProxyInfo> _client_proxy_info;
    std::unordered_map<std::string, ServiceProviderInfo> _service_provider_info;
    std::unordered_map<std::string, ServiceRequestInfo> _service_request_info;
    std::unordered_map<std::string, xtypes::DynamicType::Ptr> _message_types;

    std::size_t _next_service_call_id;
};

using EndpointPtr = std::unique_ptr<Endpoint>;

//==============================================================================
/**
 * @brief Convenience function used to create a TopicPublisher.
 *        It allows to specify both static topics and runtime generated,
 *        using the StringTemplate format.
 *
 * @param[in] topic The topic name.
 *
 * @param[in] message_type The Dynamic Type representation of the topic.
 *
 * @param[in] id The publisher ID.
 *
 * @param[in] configuration The *YAML* configuration, as speicified in the
 *            Integration Service configuration file, used to configure this
 *            TopicPublisher.
 *
 * @param[in] endpoint The *WebSocket* Endpoint.
 *
 * @returns An Integration Service TopicPublisher.
 */
std::shared_ptr<TopicPublisher> make_topic_publisher(
        const std::string& topic,
        const xtypes::DynamicType& message_type,
        const std::string& id,
        const YAML::Node& configuration,
        Endpoint& endpoint);

/**
 * @brief Convenience function used to create a ServiceProvider.
 *        It allows to specify both static services and runtime generated,
 *        using the StringTemplate format.
 *
 * @param[in] service The service name.
 *
 * @param[in] endpoint The *WebSocket* Endpoint.
 *
 * @returns An Integration Service ServiceProvider.
 */
std::shared_ptr<ServiceProvider> make_service_provider(
        const std::string& service,
        Endpoint& endpoint);

} //  namespace websocket
} //  namespace sh
} //  namespace is
} //  namespace eprosima

#endif //  _WEBSOCKET_IS_SH__SRC__ENDPOINT_HPP_
