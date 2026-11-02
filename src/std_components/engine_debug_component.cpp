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
 * @file engine_debug_component.cpp
 * @brief Implementation of the EngineDebugComponent. THIS FILE SHALL ONLY BE
 * CALLED BY CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 */

#include "engine_debug_component.hpp"

#include <sinuca3.hpp>

static const unsigned long REQUEST_MARKER = 0xA1UL;
static const unsigned long RESPONSE_MARKER = 0xB1UL;

int EngineDebugComponent::Configure(Config config) {
    bool failNow = false;
    if (config.Bool("failNow", &failNow)) return 1;
    if (failNow) return config.Error("failNow", "it's true");
    if (config.String("name", &this->name)) return 1;

    if (this->other == NULL &&
        config.ComponentReference("pointer", &this->other)) {
        return 1;
    }
    if (config.ComponentReference("fetch", &this->fetch)) {
        return 1;
    };

    if (this->other != NULL) this->otherConnectionID = this->other->Connect(1);
    if (this->fetch != NULL) this->fetchConnectionID = this->fetch->Connect(0);

    return 0;
}

void EngineDebugComponent::Clock() {
    if (this->fetch != NULL) {
        FetchPacket packet;
        packet.request = 0;
        // SINUCA3_DEBUG_PRINTF("%p: Fetching instruction!\n", this);
        this->fetch->SendRequest(this->fetchConnectionID, &packet);
        if (this->fetch->ReceiveResponse(this->fetchConnectionID, &packet) ==
            0) {
            // SINUCA3_DEBUG_PRINTF("%p: Received instruction %s\n", this,
            //                      packet.response.staticInfo->instMnemonic);
        }
    }

    if (strcmp(this->name, "") != 0) {
        SINUCA3_DEBUG_PRINTF("Component: %s | Clock!\n", this->name);
    }

    InstructionPacket messageInput = {NULL, DynamicInstructionInfo(), 0};
    InstructionPacket messageOutput = {NULL, DynamicInstructionInfo(), 0};

    if (strcmp(this->name, "A") == 0) {
        if (!(this->send)) {
            messageInput.nextInstruction = REQUEST_MARKER;
            int firstSend = this->other->SendRequest(this->otherConnectionID,
                                                     &messageInput);
            int secondSend = this->other->SendRequest(this->otherConnectionID,
                                                      &messageInput);
            if (firstSend != 0 || secondSend != 1) {
                SINUCA3_ERROR_PRINTF(
                    "Linkable request buffer test failed: first=%d "
                    "second=%d.\n",
                    firstSend, secondSend);
            } else {
                SINUCA3_DEBUG_PRINTF(
                    "PASS: request buffer rejects the second message when "
                    "full.\n");
                this->requestBufferTested = true;
            }
            this->send = true;
        } else if (!this->responseReceived) {
            if (this->other->ReceiveResponse(this->otherConnectionID,
                                             &messageOutput) == 0) {
                if (messageOutput.nextInstruction != RESPONSE_MARKER) {
                    SINUCA3_ERROR_PRINTF(
                        "Linkable response marker test failed: got=%lu.\n",
                        messageOutput.nextInstruction);
                } else {
                    SINUCA3_DEBUG_PRINTF(
                        "PASS: response arrived one cycle after C sent it.\n");
                    this->responseReceived = true;
                }
            }
        }
    } else if (strcmp(this->name, "C") == 0) {
        for (long i = 0; i < this->GetNumberOfConnections(); ++i) {
            if (this->ReceiveRequestFromConnection(i, &messageOutput) == 0) {
                if (messageOutput.nextInstruction != REQUEST_MARKER) {
                    SINUCA3_ERROR_PRINTF(
                        "Linkable request marker test failed: got=%lu.\n",
                        messageOutput.nextInstruction);
                    continue;
                }

                if (!this->requestBufferTested) {
                    SINUCA3_ERROR_PRINTF(
                        "Linkable request was received before buffer test "
                        "completed.\n");
                } else {
                    SINUCA3_DEBUG_PRINTF(
                        "PASS: request became visible to C in the next "
                        "cycle.\n");
                }

                messageInput.nextInstruction = RESPONSE_MARKER;
                int firstResponse =
                    this->SendResponseToConnection(i, &messageInput);
                int secondResponse =
                    this->SendResponseToConnection(i, &messageInput);
                if (firstResponse != 0 || secondResponse != 1) {
                    SINUCA3_ERROR_PRINTF(
                        "Linkable response buffer test failed: first=%d "
                        "second=%d.\n",
                        firstResponse, secondResponse);
                } else if (!this->responseBufferTested) {
                    SINUCA3_DEBUG_PRINTF(
                        "PASS: response buffer rejects the second message when "
                        "full.\n");
                    this->responseBufferTested = true;
                }
            }
        }
    }
}

void EngineDebugComponent::PrintStatistics() {
    SINUCA3_LOG_PRINTF("printing statistics\n");
}

EngineDebugComponent::~EngineDebugComponent() {}

#endif  // NDEBUG
