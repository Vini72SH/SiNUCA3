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
        unsigned int newPhysicalRegisterD;
        unsigned int oldPhysicalRegisterD;
        unsigned int sourcePhysicalRegister1;
        unsigned int sourcePhysicalRegister2;
        bool isFloat;
        bool dispatched;
        bool executed;
        bool valid;
    } RobEntry;

    unsigned int ptr;
    unsigned int robSize;
    unsigned int start, end;
    unsigned int occupation;
    unsigned long robEntrySize;
    RobEntry* rob;

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
    int Dequeue(RobEntry* output);

    /**
     * @brief Retrieves the first element without removing it from the Buffer.
     * Set a pointer to the first element
     * @param output A pointer to the memory region where the element
     * will be returned.
     * @return The idx of the element if sucessfuly, -1 otherwise.
     */
    int GetFirstElement(RobEntry* output);

    /**
     * @brief Advance the pointer to the next element and returns it
     * @param output A pointer to the memory region where the element will be
     * returned.
     * @return The idx of the element if sucessfuly, -1 otherwise.
     */
    int GetNextElement(RobEntry* output);

    /**
     * @brief Removes the element contained in the "base" of the
     * Buffer without returning it.
     */
    void Pop();

  public:
    ReorderBuffer()
        : ptr(0), robSize(0), start(0), end(0), occupation(0), rob(NULL) {}

    inline bool IsFull() { return this->occupation == this->robSize; }
    inline bool IsEmpty() { return this->occupation == 0; }

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
     * @param spr1 Index of the source physical register 1 used for this
     * instruction
     * @param spr2 Index of the source physical register 2 used for this
     * instruction
     * @param isFloat 1 if uses floating-point registers
     * @return -1 if the RoB is full, otherwise, the rob idx.
     */
    int Insert(const InstructionPacket instruction, unsigned int newprd,
               unsigned int oldprd, unsigned int spr1, unsigned int spr2,
               bool isFloat);

    /**
     * @brief Mark a RoB entry as executed.
     * @param idx The index of entry.
     */
    void SetExecuted(unsigned int idx);

    /**
     * @brief Get the instruction from the "Head" of RoB.
     * @param instruction A pointer to an Instruction Packet; the instruction
     * will be written to it.
     * @param newprd A pointer to an int; the new physical register will be
     * written to it.
     * @param oldprd A pointer to an int; the old physical register will be
     * written to it.
     * @param spr1 A pointer to an int; the source physical register 1 will be
     * written to it.
     * @param spr2 A pointer to an int; the source physical register 2 will be
     * written to it.
     * @param dispatched A pointer to a boolean; The info about tthe dispatch
     * will be written to it.
     * @param executed A pointer to a boolean; The state of the instruction will
     * be written to it.
     * @return The rob idx of the instruction
     */
    int GetRobFirstInstruction(InstructionPacket* instruction,
                               unsigned int* newprd, unsigned int* oldprd,
                               unsigned int* spr1, unsigned int* spr2,
                               bool* isFloat, bool* dispatched, bool* executed);

    /**
     * @brief Get the next instruction of RoB, you should call this after
     * calling GetRobFirstInstruction.
     */
    int GetRobNextInstruction(InstructionPacket* instruction,
                              unsigned int* newprd, unsigned int* oldprd,
                              unsigned int* spr1, unsigned int* spr2,
                              bool* isFloat, bool* dispatched, bool* executed);

    /**
     * @brief Define the instruction as dispatched.
     * @param idx The index of entry.
     */
    void DispatchInstruction(unsigned int idx);

    /**
     * @brief Commit an instruction. In a real processor, this is the moment
     * when the new register value is written. Basically, it updates Rob's
     * "Head," advancing to the next instruction.
     */
    void Commit();

    ~ReorderBuffer() {
        this->robSize = 0;
        if (rob) delete[] rob;
        rob = NULL;
    };
};

#endif
