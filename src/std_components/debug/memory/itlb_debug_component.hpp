#ifndef SINUCA3_ITLB_DEBUG_COMPONENT_HPP_
#define SINUCA3_ITLB_DEBUG_COMPONENT_HPP_

#include <cstddef>

#include "engine/component.hpp"
#ifndef NDEBUG

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
 * @file itlb_debug_component.hpp
 * @brief A component to test the itlb. THIS FILE SHALL ONLY BE INCLUDED BY
 * CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 */

#include <sinuca3.hpp>
#include <std_components/memory/itlb.hpp>

/**
 * @brief A component that serves to debug the itlb.
 */
class iTLBDebugComponent : public Component<InstructionPacket> {
  private:
    Component<FetchPacket>*
        fetch; /** @brief Another component to test fetching instructions. */
    int fetchConnectionID; /** @brief Connection ID for `fetch`. */

    Component<Address>*
        itlb;   /** @brief iTLB component to test sending requests. */
    int itlbID; /** @brief Connection ID for `itlb`. */

    bool waitingResponse;

  public:
    inline iTLBDebugComponent()
        : fetch(NULL), fetchConnectionID(-1), itlb(NULL), waitingResponse(false) {}

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();

    virtual ~iTLBDebugComponent();
};

#endif  // NDEBUG

#endif  // SINUCA3_ENGINE_ENGINE_DEBUG_COMPONENT_HPP_
