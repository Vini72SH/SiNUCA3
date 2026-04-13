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

#include <cstring>

#include "engine/default_packets.hpp"

int ReorderBuffer::Enqueue(RobEntry input) {
    if (this->IsFull()) return -1;

    int idx = this->end;
    this->robs[idx] = input;
    ++this->occupation;
    ++this->end;

    if (this->end == this->robSize) {
        this->end = 0;
    }

    return idx;
}

int ReorderBuffer::Dequeue(RobEntry *output) {
    if (!(this->IsEmpty())) {
        memcpy(output, &this->robs[this->start], this->robEntrySize);
        this->robs[this->start].valid = false;
        --this->occupation;
        ++this->start;

        if (this->start == this->robSize) {
            this->start = 0;
        }

        return 0;
    }

    memset(output, 0, this->robEntrySize);

    return 1;
}

int ReorderBuffer::GetFirstElement(RobEntry *output) {
    if (!this->IsEmpty()) {
        void *memoryAddress = (this->robs) + (this->start * this->robEntrySize);

        memcpy(output, memoryAddress, this->robEntrySize);

        return 0;
    }

    memset(output, 0, this->robEntrySize);

    return 1;
}

/**
 * @brief Removes the element contained in the "base" of the
 * Buffer without returning it.
 */
void ReorderBuffer::Pop() {
    if (!this->IsEmpty()) {
        --this->occupation;
        ++this->start;

        if (this->start == this->robSize) {
            this->start = 0;
        }
    }
}

int ReorderBuffer::Allocate(int sizeOfRob) {
    if (sizeOfRob <= 0) {
        this->robSize = defaultBufferSize;
    } else {
        this->robSize = sizeOfRob;
    };

    this->robs = new RobEntry[this->robSize]();
    this->robEntrySize = sizeof(RobEntry);
    if (!(this->robs)) return 1;

    return 0;
}

int ReorderBuffer::Insert(const InstructionPacket instruction, int newprd,
                          int oldprd) {
    RobEntry newRobEntry;

    newRobEntry.instruction = instruction;
    newRobEntry.newPhysicalRegisterD = newprd;
    newRobEntry.oldPhysicalRegisterD = oldprd;
    newRobEntry.executed = false;
    newRobEntry.valid = true;

    return this->Enqueue(newRobEntry);
}

void ReorderBuffer::SetExecuted(int idx) {
    if ((idx < 0) || (idx >= this->robSize) || !(this->robs[idx].valid)) return;

    this->robs[idx].executed = true;
}

int ReorderBuffer::GetRobFirstInstruction(InstructionPacket *instruction,
                                          int *newprd, int *oldprd,
                                          bool *executed) {
    RobEntry head;

    if (this->GetFirstElement(&head)) return 1;

    memcpy(instruction, &head.instruction, sizeof(InstructionPacket));
    (*newprd) = head.newPhysicalRegisterD;
    (*oldprd) = head.oldPhysicalRegisterD;
    (*executed) = head.executed;

    return (head.valid != 0);
}

void ReorderBuffer::Commit() {
    if (!(this->robs[this->start].executed)) return;
    this->robs[this->start].valid = false;

    if (this->IsEmpty()) return;

    --this->occupation;
    ++this->start;

    if (this->start == this->robSize) {
        this->start = 0;
    }
}