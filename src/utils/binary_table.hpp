#ifndef SINUCA3_UTILS_BINARY_TABLE_HPP_
#define SINUCA3_UTILS_BINARY_TABLE_HPP_

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

const int defaultTableSize = MAX_REGISTERS;

/**
 * @brief It is a binary table.
 */
class BinaryTable {
  private:
    unsigned int iterator;
    unsigned int tableSize;
    bool* bins;

  public:
    BinaryTable() : iterator(0), tableSize(0), bins(NULL) {}

    /**
     * @brief Allocate the Binary Table
     * @param sizeOfTable self-explanatory.
     * @return 0 if Ok, 1 otherwise.
     */
    int Allocate(int sizeOfTable);

    inline bool GetBin(unsigned int idx) {
        if ((this->bins) && (idx < this->tableSize))
            return this->bins[idx];
        else
            return false;
    }

    inline void SetBin(unsigned int idx) {
        if ((this->bins) && (idx < this->tableSize)) this->bins[idx] = 1;
    };

    inline void ResetBin(unsigned int idx) {
        if ((this->bins) && (idx < this->tableSize)) this->bins[idx] = 0;
    };

    inline int GetIterator() { return this->iterator; };

    inline bool GetElementIterator() { return this->bins[this->iterator]; };

    inline void Next() {
        this->iterator = (this->iterator + 1) % this->tableSize;
    };

    ~BinaryTable() {
        this->tableSize = 0;
        if (this->bins) delete[] this->bins;
        this->bins = NULL;
    };
};

#endif
