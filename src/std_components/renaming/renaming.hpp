#ifndef SINUCA3_RENAMING_HPP_
#define SINUCA3_RENAMING_HPP_

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

#include <map>

#include "config/config.hpp"
#include "engine/component.hpp"
#include "engine/default_packets.hpp"
#include "std_components/fetch/boom_fetch.hpp"
#include "std_components/renaming/reorder_buffer.hpp"
#include "utils/binary_table.hpp"
#include "utils/circular_buffer.hpp"

const int MAP_MAX_SIZE = MAX_REGISTERS;
const int DEFAULT_INT_PHYSICAL_REGISTERS = 96;
const int DEFAULT_FP_PHYSICAL_REGISTERS = 64;

enum RenamingPacketType { RENAME };

struct RenamingPacket {
    union {
        InstructionPacket instruction;
    } data;
    RenamingPacketType type;
};

/**
 * @brief The Renaming Component is responsible for handling with
 * hazards and improve IPC.
 * @details It stores and manages the states of the physical registers,
 * sending renamed instructions to the next stages of the pipeline.
 */
class Renaming : public Component<RenamingPacket> {
  private:
    BoomFetch* fetcher;
    std::map<unsigned short, unsigned int> mapTable;
    BinaryTable intBusyTable;
    BinaryTable fpBusyTable;
    BinaryTable intFreeTable;
    BinaryTable fpFreeTable;
    ReorderBuffer rob;
    CircularBuffer packets;

    unsigned int fetcherID;
    unsigned int numMapRegisters;
    unsigned int numIntPhysicalRegisters;
    unsigned int numFpPhysicalRegisters;
    unsigned int totalPhysicalRegisters;
    unsigned int robSize;

    int RenameInstruction(const InstructionPacket packet);

    void PacketBuffering();
    void PacketHandler();

  public:
    Renaming()
        : fetcher(NULL),
          fetcherID(-1),
          numMapRegisters(MAP_MAX_SIZE),
          numIntPhysicalRegisters(0),
          numFpPhysicalRegisters(0),
          totalPhysicalRegisters(0),
          robSize(0) {}

    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();

    ~Renaming() {}
};

#endif
