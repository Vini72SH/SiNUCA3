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
 * @file binary_table.cpp
 * @brief Implementation of an Binary Table.
 */

#include "binary_table.hpp"

int BinaryTable::Allocate(int sizeOfTable) {
    this->bins = new bool[sizeOfTable]();

    if (!(bins)) return 1;

    this->tableSize = sizeOfTable;

    return 0;
};
