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
extern "C" {
#include <stdint.h>
}

void AddressMapper::Clock() {
    // Todo: finish implementation of the address mapper together with mmu and
    // page table components.
}

int AddressMapper::Configure(const FetchBuffer* fetchBuffers, long numFetchers,
                             bool enablePageFrameMapping) {
    this->pageFrameMappingEnabled = enablePageFrameMapping;
    this->totalContexts = numFetchers;
    this->contextToAddressSpace = new int[this->totalContexts];

    const void* lastApplication = NULL;
    int space = -1;
    for (long i = 0; i < numFetchers; ++i) {
        const void* application = fetchBuffers[i].GetTracer();
        if (application != lastApplication) space++;
        lastApplication = application;
        this->contextToAddressSpace[i] = space;
    }

    for (long i = 0; i < this->totalContexts; ++i) {
        SINUCA3_LOG_PRINTF("Context [%d] mapped to address space [%d].\n", i,
                           this->contextToAddressSpace[i]);
    }

    return 0;
}

bool AddressMapper::GetMappingByContextInsertion(unsigned long address,
                                                 int context,
                                                 unsigned long* mappedAddress) {
    unsigned long contextFrame = this->contextToAddressSpace[context];
    unsigned long addressMask = (1UL << 48) - 1;
    *mappedAddress = (address & addressMask) | (contextFrame << 48);
    return true;
}

bool AddressMapper::GetMappingByPageFrame(unsigned long address, int context,
                                          unsigned long* mappedAddress) {
    // Todo: implementation for page/frame mapping
    (void)address;
    (void)context;
    (void)mappedAddress;
    return false;
}

bool AddressMapper::GetAddressMapping(unsigned long address, int context,
                                      unsigned long* mappedAddress) {
    if (this->pageFrameMappingEnabled) {
        return this->GetMappingByPageFrame(address, context, mappedAddress);
    } else {
        return this->GetMappingByContextInsertion(address, context,
                                                  mappedAddress);
    }
}