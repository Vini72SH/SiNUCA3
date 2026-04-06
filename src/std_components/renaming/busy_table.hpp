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
 * @brief It is a table that tracks the current status of all physical
 * registers.
 */
class BusyTable {
  private:
    int tableSize;
    bool *busies;

  public:
    BusyTable() : tableSize(0), busies(NULL) {}

    /**
     * @brief Allocate the Busy Table
     * @param sizeOfTable self-explanatory.
     * @return 0 if Ok, 1 otherwise.
     */
    int Allocate(int sizeOfTable);

    inline bool IsBusy(int idx) {
        if ((busies) && (idx < tableSize))
            return this->busies[idx];
        else
            return false;
    }

    inline void SetBusy(int idx) {
        if ((busies) && (idx < tableSize)) this->busies[idx] = 1;
    };

    inline void ResetBusy(int idx) {
        if ((busies) && (idx < tableSize)) this->busies[idx] = 0;
    };

    ~BusyTable() {
        this->tableSize = 0;
        if (busies) delete[] busies;
        busies = NULL;
    };
};

#endif