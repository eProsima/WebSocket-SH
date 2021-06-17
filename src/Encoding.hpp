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

#ifndef _WEBSOCKET_IS_SH__SRC__ENCODING_HPP_
#define _WEBSOCKET_IS_SH__SRC__ENCODING_HPP_

#include <is/core/Message.hpp>

#include <yaml-cpp/yaml.h>

#include <memory>

namespace xtypes = eprosima::xtypes;

namespace eprosima {
namespace is {
namespace sh {
namespace websocket {

/**
 * @brief Forward declaration.
 */
class Endpoint;

/**
 * @class Encoding
 *        This interface class defines all the methods that must be implemented
          in order to create an encoding to be used to construct and interpret
          raw *WebSocket* messages.
 * @details *eprosima::is::sh::websocket::JsonEncoding*: Encoding implementation for message exchanging using
 *          <a href="https://www.ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf">
 *          JSON</a> format.
 */
class Encoding
{
public:

    /**
     * @brief Interpret an incoming *WebSocket* message.
     *
     * @param[in] msg The message to be interpreted.
     *
     * @param[in] endpoint The target endpoint which will perform the actions
     *            specified by the message.
     *
     * @param[in] connection_handle Opaque pointer which identifies the current connection.
     */
    virtual void interpret_websocket_msg(
            const std::string& msg,
            Endpoint& endpoint,
            std::shared_ptr<void> connection_handle) const = 0;

    /**
     * @brief Encode a publish message.
     *
     * @param[in] topic_name The name of the topic
     *            where the message will be published to.
     *
     * @param[in] topic_type The type name of the topic
     *            where the message will be published to.
     *
     * @param[in] id The publisher ID.
     *
     * @param[in] msg The message data to be published.
     *            This will be transformed to *JSON* format beforehand.
     *
     * @returns A string representation of the encoded publication message,
     *          ready to be sent using *WebSocket*.
     */
    virtual std::string encode_publication_msg(
            const std::string& topic_name,
            const std::string& topic_type,
            const std::string& id,
            const xtypes::DynamicData& msg) const = 0;

    /**
     * @brief Encode a service response message.
     *
     * @param[in] service_name The name of the service which is answering.
     *
     * @param[in] service_type The type name of the service reply.
     *
     * @param[in] id The service ID.
     *
     * @param[in] response The message data containing the service response.
     *            This will be transformed to *JSON* format beforehand.
     *
     * @param[in] result Indicates if the response was received or not from the service server.
     *
     * @returns A string representation of the encoded service response message,
     *          ready to be sent using *WebSocket*.
     */
    virtual std::string encode_service_response_msg(
            const std::string& service_name,
            const std::string& service_type,
            const std::string& id,
            const xtypes::DynamicData& response,
            bool result) const = 0;

    /**
     * @brief Encode a subscription message.
     *
     * @param[in] topic_name The name of the topic
     *            to which the subscription will be performed.
     *
     * @param[in] message_type The type name of the topic
     *            to which the subscription will be performed.
     *
     * @param[in] id The subscriber ID.
     *
     * @param[in] configuration Additional configuration that might be required
     *            for the subscription operation.
     *
     * @returns A string representation of the encoded subscription message,
     *          ready to be sent using *WebSocket*.
     */
    virtual std::string encode_subscribe_msg(
            const std::string& topic_name,
            const std::string& message_type,
            const std::string& id,
            const YAML::Node& configuration) const = 0;

    /**
     * @brief Encode an advertisement message.
     *        This step is required prior to publish operation.
     *
     * @param[in] topic_name The name of the topic
     *            to which the advertisement will be performed.
     *
     * @param[in] message_type The type name of the topic
     *            to which the advertisement will be performed.
     *
     * @param[in] id The publisher ID.
     *
     * @param[in] configuration Additional configuration that might be required
     *            for the advertise operation.
     *
     * @returns A string representation of the encoded advertise message,
     *          ready to be sent using *WebSocket*.
     */
    virtual std::string encode_advertise_msg(
            const std::string& topic_name,
            const std::string& message_type,
            const std::string& id,
            const YAML::Node& configuration) const = 0;

    /**
     * @brief Encode a call service message.
     *
     * @param[in] service_name The name of service to be called.
     *
     * @param[in] service_type The type name of the service to be called.
     *
     * @param[in] service_request The data of the request message.
     *            This will be transformed to *JSON* format beforehand.
     *
     * @param[in] id The service ID.
     *
     * @param[in] configuration Additional configuration that might be required
     *            for the call service operation.
     *
     * @returns A string representation of the encoded call service message,
     *          ready to be sent using *WebSocket*.
     */
    virtual std::string encode_call_service_msg(
            const std::string& service_name,
            const std::string& service_type,
            const xtypes::DynamicData& service_request,
            const std::string& id,
            const YAML::Node& configuration) const = 0;

    /**
     * @brief Encode an advertise service message.
              This step is required prior to service call operations.
     *
     * @param[in] service_name The name of the service
     *            to which the advertisement will be performed.
     *
     * @param[in] request_type The request type name of the service
     *            to which the advertisement will be performed.
     *
     * @param[in] reply_type The reply type name of the service
     *            to which the advertisement will be performed.
     *
     * @param[in] id The service ID.
     *
     * @param[in] configuration Additional configuration
     *            that might be required for the advertise operation.
     *
     * @returns A string representation of the encoded service advertise message,
     *          ready to be sent using *WebSocket*.
     */
    virtual std::string encode_advertise_service_msg(
            const std::string& service_name,
            const std::string& request_type,
            const std::string& reply_type,
            const std::string& id,
            const YAML::Node& configuration) const = 0;

    /**
     * @brief Add a type to the types database.
     *
     * @param[in] type The dynamic type to be added.
     *
     * @param[in] type_name The type name.
     *
     * @returns `true` if the type was correctly added, or `false` otherwise.
     */
    virtual bool add_type(
            const xtypes::DynamicType& type,
            const std::string& type_name = "")
    {
        (void)type;
        (void)type_name;
        return false;
    }

};

using EncodingPtr = std::shared_ptr<Encoding>;

//==============================================================================
/**
 * @brief This method declaration must exist and be unique for every registered encoding.
 *        By default, only `json_encoding` is provided, but users can implement their own encoding.
 */
EncodingPtr make_json_encoding();

} //  namespace websocket
} //  namespace sh
} //  namespace is
} //  namespace eprosima

#endif //  _WEBSOCKET_IS_SH__SRC__ENCODING_HPP_
