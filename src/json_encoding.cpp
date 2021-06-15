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

#include "Encoding.hpp"
#include "Endpoint.hpp"

#include <is/json-xtypes/conversion.hpp>
#include <is/json-xtypes/json.hpp>

#include <unordered_set>

namespace eprosima {
namespace is {
namespace sh {
namespace websocket {

using json_xtypes::Json;
using nlohmann::detail::value_t;

static utils::Logger logger("is::sh::WebSocket::JsonEncoding");

//==============================================================================
// message fields
const std::string JsonOpKey = "op";
const std::string JsonIdKey = "id";
const std::string JsonTopicNameKey = "topic";
const std::string JsonTypeNameKey = "type";
const std::string JsonRequestTypeNameKey = "request_type";
const std::string JsonReplyTypeNameKey = "reply_type";
const std::string JsonMsgKey = "msg";
const std::string JsonServiceKey = "service";
const std::string JsonArgsKey = "args";
const std::string JsonValuesKey = "values";
const std::string JsonResultKey = "result";


// op codes
const std::string JsonOpAdvertiseTopicKey = "advertise";
const std::string JsonOpUnadvertiseTopicKey = "unadvertise";
const std::string JsonOpPublishKey = "publish";
const std::string JsonOpSubscribeKey = "subscribe";
const std::string JsonOpUnsubscribeKey = "unsubscribe";
const std::string JsonOpServiceRequestKey = "call_service";
const std::string JsonOpAdvertiseServiceKey = "advertise_service";
const std::string JsonOpUnadvertiseServiceKey = "unadvertise_service";
const std::string JsonOpServiceResponseKey = "service_response";

// idl of ROSBRIDGE PROTOCOL messages
const std::string idl_messages =
        R"(
struct fragment
{
    string id;
    string data;
    int32 num;
    int32 total;
};

struct png
{
    string id;
    string data;
    int32 num;
    int32 total;
};

struct cbor
{
    sequence<int8> data;
};

struct set_level
{
    string id;
    string level;
};

struct status
{
    string id;
    string level;
    string msg;
};

struct auth
{
    string mac;
    string client;
    string dest;
    string rand;
    int32 t;
    string level;
    int32 end;
};

struct advertise
{
    string id;
    string topic;
    string type;
};

struct unadvertise
{
    string id;
    string topic;
};

struct publish
{
    string id;
    string topic;
    string msg;
};

struct subscribe
{
    string id;
    string topic;
    string type;
    int32 throttle_rate;
    int32 queue_length;
    int32 fragment_size;
    string compression;
};

struct unsubscribe
{
    string id;
    string topic;
};

struct call_service
{
    string id;
    string service;
    sequence<string> args;
    int32 fragment_size;
    string compression;
};

struct advertise_service
{
    string type;
    string service;
};

struct unadvertise_service
{
    string service;
};

struct service_response
{
    string id;
    string service;
    sequence<string> values;
    boolean result;
};

)";

// This function patches the problem with types, which do not admit '/' in their type name.
std::string transform_type(
        const std::string& message_type)
{
    std::string type = message_type;

    for (size_t i = type.find('/'); i != std::string::npos; i = type.find('/', i))
    {
        type.replace(i, 1, "__");
    }

    return type;
}

//==============================================================================
static void throw_missing_key(
        const Json& object,
        const std::string& key)
{
    const std::string op_code = object.at("op").get<std::string>();

    logger << utils::Logger::Level::ERROR
           << "Incoming WebSocket message [[ " << object.dump() << " ]] with op code '"
           << op_code << "' is missing the required field '" << key << "'" << std::endl;
}

//==============================================================================
static std::string get_optional_string(
        const Json& object,
        const std::string& key)
{
    const auto it = object.find(key);

    if (it == object.end())
    {
        return "";
    }
    else
    {
        std::string temp_str = it.value().dump();
        if (temp_str.find("\"") == 0)
        {
            return temp_str.substr(1, temp_str.size() - 2);
        }
        return temp_str;
    }
}

//==============================================================================
static std::string get_required_string(
        const Json& object,
        const std::string& key)
{
    const auto it = object.find(key);
    if (it == object.end())
    {
        throw_missing_key(object, key);
    }

    std::string temp_str = it.value().dump();
    if (temp_str.find("\"") == 0)
    {
        return temp_str.substr(1, temp_str.size() - 2);
    }
    return temp_str;
}

//==============================================================================
static bool get_required_msg(
        const Json& object,
        xtypes::DynamicData& dynamicdata,
        const std::string& key)
{
    const auto it = object.find(key);
    if (it == object.end())
    {
        throw_missing_key(object, key);
    }

    try
    {
        dynamicdata = json_xtypes::convert(dynamicdata.type(), it.value());
        return true;
    }
    catch (const json_xtypes::UnsupportedType& unsupported)
    {
        logger << utils::Logger::Level::ERROR
               << "Failed to get the required message because its type is unsupported: '"
               << dynamicdata.type().name() << "', reason: [[ " << unsupported.what()
               << " ]]" << std::endl;

        return false;
    }
    catch (const json_xtypes::Json::exception& exception)
    {
        logger << utils::Logger::Level::ERROR
               << "Failed to get the required message for type '"
               << dynamicdata.type().name() << "' because conversion from xTypes"
               << " to JSON failed. Details: [[ " << exception.what() << " ]]" << std::endl;

        return false;
    }
}

//==============================================================================
/**
 * @brief Encoding implementation for message exchanging using
 * <a href="https://www.ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf">
 * JSON</a> format.
 */
class JsonEncoding : public Encoding
{
public:

    JsonEncoding()
    {
        types_ = xtypes::idl::parse(idl_messages).get_all_types();
    }

    void interpret_websocket_msg(
            const std::string& msg_str,
            Endpoint& endpoint,
            std::shared_ptr<void> connection_handle) const override
    {
        Json msg;
        try
        {
            msg = Json::parse(msg_str);
        }
        catch (const Json::exception& e)
        {
            logger << utils::Logger::Level::ERROR
                   << "Failed to parse raw received WebSocket message as a JSON: [[ "
                   << msg_str << " ]]" << std::endl;
            return;
        }

        const auto op_it = msg.find(JsonOpKey);
        if (op_it == msg.end())
        {
            logger << utils::Logger::Level::ERROR
                   << "Incoming message [[ " << msg_str << " ]] was missing the required 'op' code"
                   << std::endl;
            return;
        }

        const std::string& op_str = op_it.value().get<std::string>();

        xtypes::DynamicType::Ptr type_ptr;

        auto type_it = types_.find(op_str);
        if (type_it != types_.end())
        {
            type_ptr = type_it->second;
        }

        // Publish is the most likely type of message to be received, so we'll check
        // for that type first.
        if (op_str == JsonOpPublishKey)
        {
            std::string topic_name = get_required_string(msg, JsonTopicNameKey);
            const xtypes::DynamicType* dest_type = get_type_by_topic(topic_name);
            if (nullptr == dest_type)
            {
                return;
            }

            xtypes::DynamicData dest_data(*dest_type);
            if (get_required_msg(msg, dest_data, JsonMsgKey))
            {
                endpoint.receive_publication_ws(
                    topic_name,
                    dest_data,
                    std::move(connection_handle));
            }
            return;
        }

        // A service request is the roughly the second/third most likely type of
        // message to be received, so we'll check for that type next.
        if (op_str == JsonOpServiceRequestKey)
        {
            std::string service_name = get_required_string(msg, JsonServiceKey);
            const xtypes::DynamicType* dest_type = get_req_type_from_service(service_name);
            if (nullptr == dest_type)
            {
                return;
            }

            xtypes::DynamicData dest_data(*dest_type);
            if (get_required_msg(msg, dest_data, JsonArgsKey))
            {
                endpoint.receive_service_request_ws(
                    service_name,
                    dest_data,
                    get_optional_string(msg, JsonIdKey),
                    std::move(connection_handle));
            }
            return;
        }

        // A service response is the roughly the second/third most likely type of
        // message to be received, so we'll check for that type next.
        if (op_str == JsonOpServiceResponseKey)
        {
            std::string service_name = get_required_string(msg, JsonServiceKey);
            const xtypes::DynamicType* dest_type = get_rep_type_from_service(service_name);
            if (nullptr == dest_type)
            {
                return;
            }

            xtypes::DynamicData dest_data(*dest_type);
            if (get_required_msg(msg, dest_data, JsonValuesKey))
            {
                endpoint.receive_service_response_ws(
                    get_required_string(msg, JsonServiceKey),
                    dest_data,
                    get_optional_string(msg, JsonIdKey),
                    std::move(connection_handle));
            }
            return;
        }

        if (op_str == JsonOpAdvertiseTopicKey)
        {
            const xtypes::DynamicType* topic_type = get_type(get_required_string(msg, JsonTypeNameKey));
            if (nullptr == topic_type)
            {
                return;
            }

            endpoint.receive_topic_advertisement_ws(
                get_required_string(msg, JsonTopicNameKey),
                *topic_type,
                get_optional_string(msg, JsonIdKey),
                std::move(connection_handle));
            return;
        }

        if (op_str == JsonOpUnadvertiseTopicKey)
        {
            endpoint.receive_topic_unadvertisement_ws(
                get_required_string(msg, JsonTopicNameKey),
                get_optional_string(msg, JsonIdKey),
                std::move(connection_handle));
            return;
        }

        if (op_str == JsonOpSubscribeKey)
        {
            const xtypes::DynamicType* topic_type = get_type(get_optional_string(msg, JsonTypeNameKey));
            if (nullptr == topic_type)
            {
                return;
            }

            endpoint.receive_subscribe_request_ws(
                get_required_string(msg, JsonTopicNameKey),
                topic_type,
                get_optional_string(msg, JsonIdKey),
                std::move(connection_handle));
            return;
        }

        if (op_str == JsonOpUnsubscribeKey)
        {
            endpoint.receive_unsubscribe_request_ws(
                get_required_string(msg, JsonTopicNameKey),
                get_optional_string(msg, JsonIdKey),
                std::move(connection_handle));
            return;
        }

        if (op_str == JsonOpAdvertiseServiceKey)
        {
            const xtypes::DynamicType* req_type = get_type(get_required_string(msg, JsonRequestTypeNameKey));
            const xtypes::DynamicType* reply_type = get_type(get_required_string(msg, JsonReplyTypeNameKey));
            if (nullptr == req_type || nullptr == reply_type)
            {
                return;
            }

            endpoint.receive_service_advertisement_ws(
                get_required_string(msg, JsonServiceKey),
                *req_type,
                *reply_type,
                std::move(connection_handle));

            types_by_service_[get_required_string(msg, JsonServiceKey)] =
                    std::pair<std::string, std::string>(
                transform_type(get_required_string(msg, JsonRequestTypeNameKey)),
                transform_type(get_required_string(msg, JsonReplyTypeNameKey)));
            return;
        }

        if (op_str == JsonOpUnadvertiseServiceKey)
        {
            const xtypes::DynamicType* topic_type = get_type(get_optional_string(msg, JsonTypeNameKey));
            if (nullptr == topic_type)
            {
                return;
            }

            endpoint.receive_service_unadvertisement_ws(
                get_required_string(msg, JsonServiceKey),
                topic_type,
                std::move(connection_handle));
            return;
        }

        logger << utils::Logger::Level::ERROR
               << "Unrecognized operation: '" << op_str << "'" << std::endl;
    }

    std::string encode_publication_msg(
            const std::string& topic_name,
            const std::string& topic_type,
            const std::string& id,
            const xtypes::DynamicData& msg) const override
    {
        try
        {
            Json output;
            output[JsonOpKey] = JsonOpPublishKey;
            output[JsonTopicNameKey] = topic_name;
            output[JsonMsgKey] = json_xtypes::convert(msg);
            if (!id.empty())
            {
                output[JsonIdKey] = id;
            }

            types_by_topic_[topic_name] = transform_type(topic_type);

            return output.dump();
        }
        catch (const json_xtypes::UnsupportedType& unsupported)
        {
            logger << utils::Logger::Level::ERROR
                   << "Failed to encode publication message for topic '" << topic_name
                   << "' because its type '" << topic_type << "' is unsupported,"
                   << " reason: [[ " << unsupported.what() << " ]]" << std::endl;

            return std::string();
        }
        catch (const json_xtypes::Json::exception& exception)
        {
            logger << utils::Logger::Level::ERROR
                   << "Failed to encode publication message for topic '" << topic_name
                   << "' with type '" << topic_type << "' because conversion from xTypes"
                   << " to JSON failed. Details: [[ " << exception.what() << " ]]" << std::endl;

            return std::string();
        }
    }

    std::string encode_service_response_msg(
            const std::string& service_name,
            const std::string& service_type,
            const std::string& id,
            const xtypes::DynamicData& response,
            const bool result) const override
    {
        try
        {
            Json output;
            output[JsonOpKey] = JsonOpServiceResponseKey;
            output[JsonServiceKey] = service_name;
            output[JsonValuesKey] = json_xtypes::convert(response);
            output[JsonResultKey] = result;
            if (!id.empty())
            {
                output[JsonIdKey] = id;
            }

            auto it = types_by_service_.find(service_name);
            if (it != types_by_service_.end())
            {
                types_by_service_[service_name] = std::pair<std::string, std::string>(
                    it->second.first, transform_type(service_type));
            }
            else
            {
                types_by_service_[service_name] = std::pair<std::string, std::string>(
                    "", transform_type(service_type));
            }

            return output.dump();
        }
        catch (const json_xtypes::UnsupportedType& unsupported)
        {
            logger << utils::Logger::Level::ERROR
                   << "Failed to encode service response message for service '" << service_name
                   << "' because its type '" << service_type << "' is unsupported,"
                   << " reason: [[ " << unsupported.what() << " ]]" << std::endl;

            return std::string();
        }
        catch (const json_xtypes::Json::exception& exception)
        {
            logger << utils::Logger::Level::ERROR
                   << "Failed to encode service response message for service '" << service_name
                   << "' with type '" << service_type << "' because conversion from xTypes"
                   << " to JSON failed. Details: [[ " << exception.what() << " ]]" << std::endl;

            return std::string();
        }
    }

    std::string encode_subscribe_msg(
            const std::string& topic_name,
            const std::string& message_type,
            const std::string& id,
            const YAML::Node& /*configuration*/) const override
    {
        // TODO(MXG): Consider parsing the `configuration` for details like
        // throttle_rate, queue_length, fragment_size, and compression
        Json output;
        output[JsonOpKey] = JsonOpSubscribeKey;
        output[JsonTopicNameKey] = topic_name;
        output[JsonTypeNameKey] = transform_type(message_type);
        if (!id.empty())
        {
            output[JsonIdKey] = id;
        }

        types_by_topic_[topic_name] = transform_type(message_type);

        return output.dump();
    }

    std::string encode_advertise_msg(
            const std::string& topic_name,
            const std::string& message_type,
            const std::string& id,
            const YAML::Node& /*configuration*/) const override
    {
        Json output;
        output[JsonOpKey] = JsonOpAdvertiseTopicKey;
        output[JsonTopicNameKey] = topic_name;
        output[JsonTypeNameKey] = transform_type(message_type);
        if (!id.empty())
        {
            output[JsonIdKey] = id;
        }

        types_by_topic_[topic_name] = transform_type(message_type);

        return output.dump();
    }

    std::string encode_call_service_msg(
            const std::string& service_name,
            const std::string& service_type,
            const xtypes::DynamicData& service_request,
            const std::string& id,
            const YAML::Node& /*configuration*/) const override
    {
        try
        {
            // TODO(MXG): Consider parsing the `configuration` for details like
            // fragment_size and compression
            Json output;
            output[JsonOpKey] = JsonOpServiceRequestKey;
            output[JsonServiceKey] = service_name;
            output[JsonArgsKey] = json_xtypes::convert(service_request);
            if (!id.empty())
            {
                output[JsonIdKey] = id;
            }

            auto it = types_by_service_.find(service_name);
            if (it != types_by_service_.end())
            {
                types_by_service_[service_name] = std::pair<std::string, std::string>(
                    transform_type(service_type), it->second.second);
            }
            else
            {
                types_by_service_[service_name] = std::pair<std::string, std::string>(
                    transform_type(service_type), "");
            }

            return output.dump();
        }
        catch (const json_xtypes::UnsupportedType& unsupported)
        {
            logger << utils::Logger::Level::ERROR
                   << "Failed to encode service request message for service '" << service_name
                   << "' because its type '" << service_type << "' is unsupported,"
                   << " reason: [[ " << unsupported.what() << " ]]" << std::endl;

            return std::string();
        }
        catch (const json_xtypes::Json::exception& exception)
        {
            logger << utils::Logger::Level::ERROR
                   << "Failed to encode service request message for service '" << service_name
                   << "' with type '" << service_type << "' because conversion from xTypes"
                   << " to JSON failed. Details: [[ " << exception.what() << " ]]" << std::endl;

            return std::string();
        }
    }

    std::string encode_advertise_service_msg(
            const std::string& service_name,
            const std::string& request_type,
            const std::string& reply_type,
            const std::string& /*id*/,
            const YAML::Node& /*configuration*/) const override
    {
        Json output;
        output[JsonOpKey] = JsonOpAdvertiseServiceKey;
        output[JsonRequestTypeNameKey] = transform_type(request_type);
        output[JsonReplyTypeNameKey] = transform_type(reply_type);
        output[JsonServiceKey] = service_name;

        types_by_service_[service_name] = std::pair<std::string, std::string>(transform_type(
                            request_type), transform_type(reply_type));

        return output.dump();
    }

    const xtypes::DynamicType* get_type(
            const std::string& type_name) const
    {
        auto type_it = types_.find(transform_type(type_name));
        if (type_it != types_.end())
        {
            return type_it->second.get();
        }
        else
        {
            logger << utils::Logger::Level::ERROR
                   << "Incoming message refers to an unregistered type: '"
                   << type_name << "'" << std::endl;

            return nullptr;
        }
    }

    bool add_type(
            const xtypes::DynamicType& type,
            const std::string& type_name) override
    {
        std::string name = transform_type(type_name.empty() ? type.name() : type_name);
        auto result = types_.emplace(name, type);
        return result.second;
    }

    const xtypes::DynamicType* get_type_by_topic(
            const std::string& topic_name) const
    {
        return get_type(types_by_topic_[topic_name]);
    }

    const xtypes::DynamicType* get_req_type_from_service(
            const std::string& service_name) const
    {
        std::string req_type = types_by_service_[service_name].first;
        if (req_type.empty())
        {
            logger << utils::Logger::Level::ERROR
                   << "There is not any registered service request type for the service '"
                   << service_name << "'" << std::endl;
            return nullptr;
        }
        return get_type(req_type);
    }

    const xtypes::DynamicType* get_rep_type_from_service(
            const std::string& service_name) const
    {
        std::string rep_type = types_by_service_[service_name].second;
        if (rep_type.empty())
        {
            logger << utils::Logger::Level::ERROR
                   << "There is not any registered service reply type for the service '"
                   << service_name << "'" << std::endl;
            return nullptr;
        }
        return get_type(rep_type);
    }

protected:

    std::map<std::string, xtypes::DynamicType::Ptr> types_;
    mutable std::map<std::string, std::string> types_by_topic_;
    mutable std::map<std::string, std::pair<std::string, std::string> > types_by_service_;

};

//==============================================================================
EncodingPtr make_json_encoding()
{
    return std::make_shared<JsonEncoding>();
}

} //  namespace websocket
} //  namespace sh
} //  namespace is
} //  namespace eprosima
