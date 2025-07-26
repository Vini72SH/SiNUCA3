#ifndef SINUCA3_FETCHER_HPP_
#define SINUCA3_FETCHER_HPP_

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
 * @file fetcher.hpp
 * @brief API of the Fetcher, a generic component for simulating logic in the
 * first stage of a pipeline, supporting integration with predictors and caches.
 */

#include <cstdlib>

#include "../../sinuca3.hpp"

class Fetcher : public sinuca::Component<int> {
  private:
    sinuca::Component<sinuca::FetchPacket>* fetch;
    sinuca::Component<sinuca::InstructionPacket>* instructionMemory;
    sinuca::InstructionPacket* fetchBuffer;
    unsigned long fetchBufferUsage;
    unsigned long fetchSize;
    unsigned long fetchInterval;
    unsigned long fetchClock;
    int fetchID;
    int instructionMemoryID;

    int FetchConfigParameter(sinuca::config::ConfigValue value);
    int InstructionMemoryConfigParameter(sinuca::config::ConfigValue value);
    int FetchSizeConfigParameter(sinuca::config::ConfigValue value);
    int FetchIntervalConfigParameter(sinuca::config::ConfigValue value);

    void ClockSendBuffered();
    void ClockRequestFetch();
    void ClockFetch();

  public:
    inline Fetcher()
        : fetch(NULL),
          instructionMemory(NULL),
          fetchBuffer(NULL),
          fetchBufferUsage(0),
          fetchSize(1),
          fetchInterval(1),
          fetchClock(0) {}
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();

    virtual ~Fetcher();
};

#endif  // SINUCA3_FETCHER_HPP_
