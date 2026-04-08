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
 * @file reorder_buffer.cpp
 * @brief Implementation of the Reorder Buffer component.
 */

#include "reorder_buffer.hpp"

#include "engine/default_packets.hpp"
#include "utils/circular_buffer.hpp"

int ReorderBuffer::Allocate(int sizeOfRob) {
    if (sizeOfRob <= 0) {
        this->robSize = defaultBufferSize;
    } else {
        this->robSize = sizeOfRob;
    };

    this->robs = new CircularBuffer();
    if (!(this->robs)) return 1;

    this->robs->Allocate(this->robSize, sizeof(RobData));

    return 0;
}

int ReorderBuffer::Insert(const InstructionPacket instruction, int newprd,
                          int oldprd) {
    RobData newRobEntry;

    newRobEntry.instruction = instruction;
    newRobEntry.newPhysicalRegisterD = newprd;
    newRobEntry.oldPhysicalRegisterD = oldprd;
    newRobEntry.executed = false;

    if ((this->robs->Enqueue(&newRobEntry))) {
        return 1;
    }

    return 0;
}