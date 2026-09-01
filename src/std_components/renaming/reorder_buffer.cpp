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
    this->rob[idx] = input;
    ++this->occupation;
    ++this->end;

    if (this->end == this->robSize) {
        this->end = 0;
    }

    return idx;
}

int ReorderBuffer::Dequeue(RobEntry* output) {
    if (!(this->IsEmpty())) {
        memcpy(output, &this->rob[this->start], this->robEntrySize);
        this->rob[this->start].valid = false;
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

int ReorderBuffer::GetFirstElement(RobEntry* output) {
    if (!this->IsEmpty()) {
        void* memoryAddress = (this->rob) + (this->start * this->robEntrySize);
        this->ptr = 0;
        memcpy(output, memoryAddress, this->robEntrySize);

        return this->start;
    }

    memset(output, 0, this->robEntrySize);

    return -1;
}

int ReorderBuffer::GetNextElement(RobEntry* output) {
    if (!this->IsEmpty()) {
        unsigned int idx = (this->start + this->ptr + 1) % this->robSize;
        if (idx <= this->end) {
            this->ptr++;
            void* memoryAddress = (this->rob) + (idx * this->robEntrySize);
            memcpy(output, memoryAddress, this->robEntrySize);

            return idx;
        }
    }

    memset(output, 0, this->robEntrySize);

    return -1;
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

    this->rob = new RobEntry[this->robSize]();
    this->robEntrySize = sizeof(RobEntry);
    if (!(this->rob)) return 1;

    return 0;
}

int ReorderBuffer::Insert(const InstructionPacket instruction,
                          unsigned int newprd, unsigned int oldprd,
                          unsigned int spr1, unsigned int spr2, bool isFloat) {
    RobEntry newRobEntry;

    newRobEntry.instruction = instruction;
    newRobEntry.newPhysicalRegisterD = newprd;
    newRobEntry.oldPhysicalRegisterD = oldprd;
    newRobEntry.sourcePhysicalRegister1 = spr1;
    newRobEntry.sourcePhysicalRegister2 = spr2;
    newRobEntry.isFloat = isFloat;
    newRobEntry.dispatched = false;
    newRobEntry.executed = false;
    newRobEntry.valid = true;

    return this->Enqueue(newRobEntry);
}

void ReorderBuffer::SetExecuted(unsigned int idx) {
    if ((idx >= this->robSize) || !(this->rob[idx].valid)) return;

    this->rob[idx].executed = true;
}

int ReorderBuffer::GetRobFirstInstruction(InstructionPacket* instruction,
                                          unsigned int* newprd,
                                          unsigned int* oldprd,
                                          unsigned int* spr1,
                                          unsigned int* spr2, bool* isFloat,
                                          bool* dispatched, bool* executed) {
    RobEntry head;

    if (this->GetFirstElement(&head)) return 1;

    memcpy(instruction, &head.instruction, sizeof(InstructionPacket));
    (*newprd) = head.newPhysicalRegisterD;
    (*oldprd) = head.oldPhysicalRegisterD;
    (*spr1) = head.sourcePhysicalRegister1;
    (*spr2) = head.sourcePhysicalRegister2;
    (*isFloat) = head.isFloat;
    (*dispatched) = head.dispatched;
    (*executed) = head.executed;

    return (head.valid != 0);
}

int ReorderBuffer::GetRobNextInstruction(InstructionPacket* instruction,
                                         unsigned int* newprd,
                                         unsigned int* oldprd,
                                         unsigned int* spr1, unsigned int* spr2,
                                         bool* isFloat, bool* dispatched,
                                         bool* executed) {
    RobEntry next;

    if (this->GetNextElement(&next)) return 1;

    memcpy(instruction, &next.instruction, sizeof(InstructionPacket));
    (*newprd) = next.newPhysicalRegisterD;
    (*oldprd) = next.oldPhysicalRegisterD;
    (*spr1) = next.sourcePhysicalRegister1;
    (*spr2) = next.sourcePhysicalRegister2;
    (*isFloat) = next.isFloat;
    (*dispatched) = next.dispatched;
    (*executed) = next.executed;

    return (next.valid != 0);
}

void ReorderBuffer::DispatchInstruction(unsigned int idx) {
    if ((idx >= this->robSize) || !(this->rob[idx].valid)) return;

    this->rob[idx].dispatched = true;
}

void ReorderBuffer::Commit() {
    if (!(this->rob[this->start].executed)) return;
    this->rob[this->start].valid = false;

    if (this->IsEmpty()) return;

    --this->occupation;
    ++this->start;

    if (this->start == this->robSize) {
        this->start = 0;
    }
}
