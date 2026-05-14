//
// Copyright (C) 2026  HiPES - Universidade Federal do Paraná
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
 * @file address_mapper.cpp
 */

#include "address_mapper.hpp"

void AddressMapper::Clock() { }

bool AddressMapper::GetMappingByContextInsertion(unsigned long address, int context,
                           unsigned long* mappedAddress) {
    unsigned long contextFrame = context;
    unsigned long addressMask = (1UL << 48) - 1;
    *mappedAddress = (address & addressMask) | (contextFrame << 48);
    return true;
}

bool AddressMapper::GetMappingByPageFrame(unsigned long address, int context,
                        unsigned long* mappedAddress) {
    // Todo: implementation for page/frame mapping
    (void)address;  // To avoid unused parameter warning
    (void)context;
    (void)mappedAddress;
    return false;
}

bool AddressMapper::GetAddressMapping(unsigned long address, int context, unsigned long* mappedAddress) {
    if (this->pageFrameMappingEnabled) {
        return this->GetMappingByPageFrame(address, context, mappedAddress);
    } else {
        return this->GetMappingByContextInsertion(address, context, mappedAddress);
    }
}