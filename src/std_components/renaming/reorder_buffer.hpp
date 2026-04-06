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

#include "engine/default_packets.hpp"
#include "utils/circular_buffer.hpp"

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
        bool executed;
    } RobData;

    int robSize;
    CircularBuffer *robs;

  public:
    ReorderBuffer() : robSize(0), robs(NULL) {}

    /**
     * @brief Allocates the structure of a Reorder Buffer.
     * @param sizeOfRob The number of instructions supported by Rob.
     * @return 0 if Ok, 1 otherwise.
     */
    int Allocate(int sizeOfRob);

    void Insert(int newprd, int oldprd);
    void SetExecuted(int idx);
    void GetFirstElement();
    void Commit();

    ~ReorderBuffer() {
        this->robSize = 0;
        if (robs) delete robs;
        robs = NULL;
    };
};

#endif