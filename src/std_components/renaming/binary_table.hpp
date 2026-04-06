#ifndef SINUCA3_RENAMING_BUSY_TABLE_HPP_
#define SINUCA3_RENAMING_BUSY_TABLE_HPP_

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

/**
 * @brief It is a binary table.
 */
class BinaryTable {
  private:
    int iterator;
    int tableSize;
    bool *bins;

  public:
    BinaryTable() : iterator(0), tableSize(0), bins(NULL) {}

    /**
     * @brief Allocate the Busy Table
     * @param sizeOfTable self-explanatory.
     * @return 0 if Ok, 1 otherwise.
     */
    int Allocate(int sizeOfTable);

    inline bool IsBusy(int idx) {
        if ((bins) && (idx < tableSize))
            return this->bins[idx];
        else
            return false;
    }

    inline void SetBusy(int idx) {
        if ((bins) && (idx < tableSize)) this->bins[idx] = 1;
    };

    inline void ResetBusy(int idx) {
        if ((bins) && (idx < tableSize)) this->bins[idx] = 0;
    };

    inline int GetIterator() { return this->iterator; };

    inline bool GetElementIterator() { return this->bins[this->iterator]; };

    inline void Next() {
        this->iterator = (this->iterator + 1) % this->tableSize;
    };

    ~BinaryTable() {
        this->tableSize = 0;
        if (bins) delete[] bins;
        bins = NULL;
    };
};

#endif