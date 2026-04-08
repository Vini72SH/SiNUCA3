#ifndef SINUCA3_RENAMING_REORDER_BUFFER_HPP_
#define SINUCA3_RENAMING_REORDER_BUFFER_HPP_

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

#include <cstddef>
#include <cstring>

#include "engine/default_packets.hpp"

const int defaultBufferSize = MAX_REGISTERS;

/**
 * @brief The Reorder Buffer is a component that maintains a sequence of
 * instructions and enables out-of-order execution. Its role is to store a
 * limited number of instructions and commit each one once certain conditions
 * have been met.
 */
class ReorderBuffer {
  private:
    typedef struct {
        InstructionPacket instruction;
        int newPhysicalRegisterD;
        int oldPhysicalRegisterD;
        bool executed;
    } RobEntry;

    int robSize;
    int start, end;
    int occupation;
    unsigned long robEntrySize;
    RobEntry *robs;

    inline bool IsFull() { return this->occupation == this->robSize; }
    inline bool IsEmpty() { return this->occupation == 0; }

    /**
     * @brief Inserts the element at the "top" of the buffer.
     * @param input A pointer to the element to be inserted.
     * @return 0 if successfuly, 1 otherwise.
     */
    int Enqueue(RobEntry input);

    /**
     * @brief Removes and returns the element contained in the "base" of the
     * Buffer.
     * @param output A pointer to the memory region where the element
     * will be returned.
     * @return 0 if successfuly, 1 otherwise.
     */
    int Dequeue(RobEntry *output);

    /**
     * @brief Retrieves the first element without removing it from the Buffer.
     * @param output A pointer to the memory region where the element
     * will be returned.
     * @return 0 if sucessfuly, 1 otherwise.
     */
    int GetFirstElement(RobEntry *output);

    /**
     * @brief Removes the element contained in the "base" of the
     * Buffer without returning it.
     */
    void Pop();

  public:
    ReorderBuffer() : robSize(0), start(0), end(0), occupation(0), robs(NULL) {}

    /**
     * @brief Allocates the structure of a Reorder Buffer.
     * @param sizeOfRob The number of instructions supported by Rob.
     * @return 0 if Ok, 1 otherwise.
     */
    int Allocate(int sizeOfRob);

    /**
     * @brief Inserts an instruction into the RoB.
     * @param instruction Data from an instruction.
     * @param newprd Index of the new physical register used for this
     * instruction.
     * @param oldprd Index of the old physical register used for this
     * instruction
     * @return 0 if everything is ok, 1 if the RoB is full.
     */
    int Insert(const InstructionPacket instruction, int newprd, int oldprd);

    /**
     * @brief Mark a RoB entry as executed.
     * @param idx The index of entry
     */
    void SetExecuted(int idx);

    /**
     *
     */
    void GetFirstElement();

    /**
     *
     */
    void Commit();

    ~ReorderBuffer() {
        this->robSize = 0;
        if (robs) delete robs;
        robs = NULL;
    };
};

#endif