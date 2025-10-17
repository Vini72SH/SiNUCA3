#include "std_components/memory/itlb.hpp"
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
 * @file itlb_debug_component.cpp
 * @brief Implementation of a component to test the itlb. THIS FILE SHALL ONLY
 * BE INCLUDED BY CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 */

#include <cstdio>
#include <sinuca3.hpp>

#include "itlb_debug_component.hpp"

int iTLBDebugComponent::SetConfigParameter(const char* parameter,
                                           ConfigValue value) {
    if (strcmp(parameter, "fetch") == 0) {
        this->fetch = dynamic_cast<Component<FetchPacket>*>(
            value.value.componentReference);
        if (this->fetch == NULL) {
            SINUCA3_DEBUG_PRINTF(
                "%p: Failed to cast fetch to Component<InstructionPacket>.\n",
                this);
            return 1;
        }
        this->fetchConnectionID = this->fetch->Connect(0);
        SINUCA3_DEBUG_PRINTF("%p: connected to fetch: %d\n", this,
                             this->fetchConnectionID);
    }

    if (strcmp(parameter, "itlb") == 0) {
        this->itlb =
            dynamic_cast<Component<Address>*>(value.value.componentReference);
        if (this->itlb == NULL) {
            SINUCA3_DEBUG_PRINTF(
                "%p: Failed to cast itlb to Component<Address>.\n", this);
            return 1;
        }
        this->itlbID = this->itlb->Connect(0);
        SINUCA3_DEBUG_PRINTF("%p: connected to itlb: %d\n", this, this->itlbID);
    }

    return 0;
}

int iTLBDebugComponent::FinishSetup() {
    if (this->fetch == NULL) {
        SINUCA3_DEBUG_PRINTF("iTLBDebugComponent: missing fetch Component.\n");
        return 1;
    }
    if (this->itlb == NULL) {
        SINUCA3_DEBUG_PRINTF("iTLBDebugComponent: missing itlb Component.\n");
        return 1;
    }
    return 0;
}

void iTLBDebugComponent::Clock() {
    SINUCA3_DEBUG_PRINTF("Clock!\n");
    Address fakePhysicalAddress;
    if (this->waitingFor >= 2){

        if(this->itlb->ReceiveResponse(this->itlbID, &fakePhysicalAddress)){
            SINUCA3_DEBUG_PRINTF("%p: Fetcher stall\n", this);
            return;  // stall
        } else {
            this->waitingFor -= 1;
        }

    }

    FetchPacket packet;
    packet.request = 0;
    bool received = false;
    this->fetch->SendRequest(this->fetchConnectionID, &packet);
    if (this->fetch->ReceiveResponse(this->fetchConnectionID, &packet) == 0) {
        received = true;
        SINUCA3_DEBUG_PRINTF("%p: Fetched instruction %s\n", this,
                             packet.response.staticInfo->opcodeAssembly);
    }

    if(!received)
        return;

    Address virtualAddress =
        static_cast<Address>(packet.response.staticInfo->opcodeAddress);
    SINUCA3_DEBUG_PRINTF("%p: Sending request %p for itlb\n", this,
                         (void*)virtualAddress);
    this->itlb->SendRequest(this->itlbID, &virtualAddress);
    this->waitingFor += 1;
}

void iTLBDebugComponent::PrintStatistics() {
    SINUCA3_LOG_PRINTF("EngineDebugComponent %p: printing statistics\n", this);
}

iTLBDebugComponent::~iTLBDebugComponent() {}

#endif  // NDEBUG
