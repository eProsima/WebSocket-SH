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

#ifndef _WEBSOCKET_IS_SH__SRC__JWTVALIDATOR_HPP_
#define _WEBSOCKET_IS_SH__SRC__JWTVALIDATOR_HPP_

#include <jwt/jwt.hpp>

#include <regex>

namespace eprosima {
namespace is {
namespace sh {
namespace websocket {

/**
 * @class VerificationPolicy
 * @brief Class containing all the relevant information about the verification policy,
 * which includes the public key or the secret key used for generating the token.
 */
class VerificationPolicy
{
public:

    /**
     * @brief Rule signature.
     */
    using Rule = std::pair<std::string, std::string>;

    /**
     * @brief Constructor
     */
    VerificationPolicy(
            std::vector<Rule> rules,
            std::vector<Rule> header_rules,
            std::string secret_or_pubkey);

    /**
     * @brief Retrieves the public key or secret.
     */
    inline const std::string& secret_or_pubkey() const
    {
        return _secret_or_pubkey;
    }

    void check(
            const std::string& token,
            const json_t& header,
            const json_t& payload);

private:

    std::string _secret_or_pubkey;
    std::vector<Rule> _rules;
    std::vector<Rule> _header_rules;
    std::unordered_map<std::string, std::regex> _matchers;
    std::unordered_map<std::string, std::regex> _header_matchers;
};

/**
 * @class JwtValidator
 * @brief Class that validates the received <a href="https://jwt.io/">JSON Web Token</a> according to
 * the VerificationPolicy specified on the configuration file.
 */
class JwtValidator
{
public:

    /**
     * @brief Verifies the token.
     *
     * @param [in] token String containing the JSON Web Token.
     *
     * @throws VerificationError
     */
    void verify(
            const std::string& token);


    /**
     * @brief Adds a policy to resolve the verification strategy to use.
     * @details The VerificationPolicy should set the VerificationStrategy and returns true if
     * it is able to provide a strategy. If there are multiple policies that can process a token,
     * the 1st policy that matches is used. VerificationPolicyFactory contains some simple
     * predefined policies.
     * @remarks The idea is that JwtValidator should support verfiying in multiple use cases.
     * For example, choosing a secret based on the issuer or other claims and any custom strategy
     * as required. There is no way to open up such flexibility from within the class so the
     * conclusion is to have a handler that the consumer supplies to choose the verification method.
     * @param [in] policy The policy to be added.
     */
    void add_verification_policy(
            const VerificationPolicy& policy);

private:

    std::vector<VerificationPolicy> _verification_policies;
};

} //  namespace websocket
} //  namespace sh
} //  namespace is
} //  namespace eprosima

#endif //  _WEBSOCKET_IS_SH__SRC__JWTVALIDATOR_HPP_
