#ifndef SINUCA3_SIMPLE_INSTRUCTION_MEMORY_HPP_
#define SINUCA3_SIMPLE_INSTRUCTION_MEMORY_HPP_

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
 * @file simple_instruction_memory.hpp
 * @details Public API of the SimpleInstructionMemory: component for SiNUCA3
 * which just responds immediatly for every request (of instructions). I.e., the
 * perfect (instruction) memory: big and works at the light speed! Beware that
 * it connects with `sendTo` with unlimited bandwidth, and thus limits in it may
 * cause a high memory usage due to it's queue. If no `sendTo` parameter is
 * provided, it responds to the requests.
 */

#include "../../sinuca3.hpp"

class SimpleInstructionMemory
    : public sinuca::Component<sinuca::InstructionPacket> {
  private:
    sinuca::Component<sinuca::InstructionPacket>* sendTo;
    unsigned long numberOfRequests;
    int sendToID;

  public:
    inline SimpleInstructionMemory() : numberOfRequests(0) {};
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();
    ~SimpleInstructionMemory();
};

#endif  // SINUCA3_SIMPLE_INSTRUCTION_MEMORY_HPP_
