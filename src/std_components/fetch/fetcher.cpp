//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
 * @file fetcher.cpp
 * @brief API of the Fetcher, a generic component for simulating logic in the
 * first stage of a pipeline, supporting integration with predictors and caches.
 */

#include "fetcher.hpp"

#include "../../sinuca3.hpp"
#include "../../utils/logging.hpp"

int Fetcher::FinishSetup() {
    if (this->fetch == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Fetcher didn't received required parameter `fetch`.\n");
        return 1;
    }
    if (this->instructionMemory == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Fetcher didn't received required parameter "
            "`instructionMemory`.\n");
        return 1;
    }

    this->fetchID = this->fetch->Connect(this->fetchSize);
    this->instructionMemoryID =
        this->instructionMemory->Connect(this->fetchSize);

    this->fetchBuffer = new sinuca::InstructionPacket[this->fetchSize];

    return 0;
}

int Fetcher::FetchConfigParameter(sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeComponentReference) {
        this->fetch = dynamic_cast<sinuca::Component<sinuca::FetchPacket>*>(
            value.value.componentReference);
        if (this->fetch != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetch` is not a Component<FetchPacket>.\n");
    return 1;
}

int Fetcher::InstructionMemoryConfigParameter(
    sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeComponentReference) {
        this->instructionMemory =
            dynamic_cast<sinuca::Component<sinuca::InstructionPacket>*>(
                value.value.componentReference);
        if (this->instructionMemory != NULL) {
            return 0;
        }
    }

    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `instructionMemory` is not a "
        "Component<InstructionPacket>.\n");
    return 1;
}

int Fetcher::FetchSizeConfigParameter(sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchSize = value.value.integer;
            return 0;
        }
    }

    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetchSize` is not a integer > 0.\n");
    return 1;
}

int Fetcher::FetchIntervalConfigParameter(sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchInterval = value.value.integer;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetchInterval` is not a integer > 0.\n");
    return 1;
}

int Fetcher::SetConfigParameter(const char* parameter,
                                sinuca::config::ConfigValue value) {
    if (strcmp(parameter, "fetch") == 0) {
        return this->FetchConfigParameter(value);
    } else if (strcmp(parameter, "instructionMemory") == 0) {
        return this->InstructionMemoryConfigParameter(value);
    } else if (strcmp(parameter, "fetchSize") == 0) {
        return this->FetchSizeConfigParameter(value);
    } else if (strcmp(parameter, "fetchInterval") == 0) {
        return this->FetchIntervalConfigParameter(value);
    }

    SINUCA3_ERROR_PRINTF("Fetcher received unknown parameter %s.\n", parameter);

    return 1;
}

void Fetcher::ClockSendBuffered() {
    unsigned long i = 0;

    while (i < this->fetchBufferUsage &&
           (this->instructionMemory->SendRequest(this->instructionMemoryID,
                                                 &this->fetchBuffer[i]) == 0)) {
        ++i;
    }
    this->fetchBufferUsage -= i;
    if (this->fetchBufferUsage > 0) {
        memmove(&this->fetchBuffer[i], this->fetchBuffer,
                this->fetchBufferUsage * sizeof(*this->fetchBuffer));
    }
}

void Fetcher::ClockRequestFetch() {
    unsigned long fetchBufferByteUsage = 0;
    for (unsigned long i = 0; i < this->fetchBufferUsage; ++i) {
        fetchBufferByteUsage += this->fetchBuffer[i].staticInfo->opcodeSize;
    }

    sinuca::FetchPacket request;
    request.request = fetchSize - fetchBufferByteUsage;
    this->fetch->SendRequest(this->fetchID, &request);
}

void Fetcher::ClockFetch() {
    // We're guaranteed to have space because we asked only enough bytes to fill
    // the buffer. The engine is guaranteed to send only up until that amount,
    // the cycle right after we asked.
    while (
        this->fetch->ReceiveResponse(
            this->fetchID,
            (sinuca::FetchPacket*)&this->fetchBuffer[this->fetchBufferUsage]) ==
        0) {
        ++this->fetchBufferUsage;
    }
}

void Fetcher::Clock() {
    this->ClockSendBuffered();
    this->ClockFetch();

    if (this->fetchClock % this->fetchInterval == 0) {
        this->fetchClock = 0;
        this->ClockRequestFetch();
    }

    ++this->fetchClock;
}

void Fetcher::Flush() {}

void Fetcher::PrintStatistics() {}

Fetcher::~Fetcher() {
    if (this->fetchBuffer != NULL) {
        delete[] this->fetchBuffer;
    }
}
