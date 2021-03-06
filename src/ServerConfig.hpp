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

#ifndef _WEBSOCKET_IS_SH__SRC__SERVERCONFIG_HPP_
#define _WEBSOCKET_IS_SH__SRC__SERVERCONFIG_HPP_

#include <yaml-cpp/yaml.h>

#include "JwtValidator.hpp"

namespace eprosima {
namespace is {
namespace sh {
namespace websocket {

/**
 * @class ServerConfig
 * @brief Loads from the *YAML* configuration file the authentication policy that will be used by the JwtValidator.
 */
class ServerConfig
{
public:

    static bool load_auth_policy(
            JwtValidator& jwt_validator,
            const YAML::Node& auth_node);

private:

    static std::string _glob_to_regex(
            const std::string& s);
    static VerificationPolicy _parse_policy_yaml(
            const YAML::Node& policy_node);
};

} //  namespace websocket
} //  namespace sh
} //  namespace is
} //  namespace eprosima

#endif //  _WEBSOCKET_IS_SH__SRC__SERVERCONFIG_HPP_
