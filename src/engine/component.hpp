#ifndef SINUCA3_ENGINE_COMPONENT_HPP_
#define SINUCA3_ENGINE_COMPONENT_HPP_

//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file component.hpp
 * @brief Public API of the component template class.
 */

#include "../config/config.hpp"
#include "linkable.hpp"

namespace sinuca {

/**
 * @details All components shall inherit from this class. The MessageType type
 * parameter defines the message type the component receives from other
 * components. If the component does not receive any message, int can be used as
 * a placeholder. Note that Component<T> is just a wrapper for the underlying
 * mother class Linkable. This is done to centralize the message-passing
 * implementation in a non-template class for optimization reasons. This wrapper
 * allows a nice and type-safe API on top of a single, fast and generic
 * implementation.
 *
 * Avoiding big types in MessageType is a good idea, because they're passed by
 * value.
 */
template <typename MessageType>
class Component : public engine::Linkable {
  public:
    /**
     * @param messageSize The size of the message that will be used by the
     * component.
     */
    inline Component(int messageSize) : engine::Linkable(messageSize) {}

    /**
     * @brief Wrapper to SendRequestToLinkable method
     */
    inline void SendRequestToComponent(Linkable* component, int connectionID,
                                       void* message) {
        this->SendRequestToLinkable(component, connectionID, message);
    };

    /**
     * @brief Wrapper to SendResponseToLinkable method
     */
    inline void SendResponseToComponent(Linkable* component, int connectionID,
                                        void* message) {
        this->SendResponseToLinkable(component, connectionID, message);
    };

    /**
     * @brief Wrapper to ReceiveRequestFromLinkable method
     */
    inline MessageType* ReceiveRequestFromComponent(Linkable* component,
                                             int connectionID) {
        return this->ReceiveRequestFromLinkable(component, connectionID);
    };

    /**
     * @brief Wrapper to ReceiveResponseFromLinkable method
     */
    inline MessageType* ReceiveResponseFromComponent(Linkable* component,
                                              int connectionID) {
        return this->ReceiveResponseFromLinkable(component, connectionID);
    };

    /**
     * @brief Wrapper to SendRequestToConnection method
     */
    inline void SendRequestForConnection(int connectionID, void* message) {
        this->SendRequestToConnection(connectionID, message);
    };

    /**
     * @brief Wrapper to SendResponseToConnection method
     */
    inline void SendResponseForConnection(int connectionID, void* message) {
        this->SendResponseToConnection(connectionID, message);
    };

    /**
     * @brief Wrapper to ReceiveRequestFromConnection method
     */
    inline MessageType* ReceiveRequestForAConnection(int connectionID) {
        return this->ReceiveRequestFromConnection(connectionID);
    };

    /**
     * @brief Wrapper to ReceiveResponseFromConnection method
     */
    inline MessageType* ReceiveResponseForAConnection(int connectionID) {
        return this->ReceiveResponseFromConnection(connectionID);
    };

    inline ~Component() {}
};

}  // namespace sinuca

#endif  // SINUCA3_ENGINE_COMPONENT_HPP_
