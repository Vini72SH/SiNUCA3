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

/**
 * @file cache_nway.cpp
 * @brief Implementation of a abstract nway cache.
 */

#include "cacheMemory.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <utils/logging.hpp>

#include "replacement_policy.hpp"
#include "utils/cache/replacement_policies/lru.hpp"
#include "utils/cache/replacement_policies/random.hpp"
#include "utils/cache/replacement_policies/roundRobin.hpp"

using namespace CacheMemoryNS;

/**
 * @brief Number of bits in a address.
 * Default is 64 bits.
 * Used to calculate how many bits are used for tag.
 * If you want to change this, may be better to clone this file
 * and create another CacheMemory class for this.
 */
static const int addrSizeBits = 64;

static double getLog2(double x) { return log(x) / log(2); }

static bool checkIfPowerOfTwo(unsigned long x) {
    if (x == 0) return false;  // 0 is not a power of 2
    // x is power of 2 if only one bit is set.
    return (x & (x - 1)) == 0;
}

CacheMemory *CacheMemory::fromCacheSize(unsigned int cacheSize,
                                        unsigned int lineSize,
                                        unsigned int associativity,
                                        CacheMemoryNS::ReplacementPoliciesID policy) {
    unsigned int numSets = cacheSize / (lineSize * associativity);
    return CacheMemory::fromNumSets(numSets, lineSize, associativity, policy);
}

CacheMemory *CacheMemory::fromNumSets(unsigned int numSets,
                                      unsigned int lineSize,
                                      unsigned int associativity,
                                      CacheMemoryNS::ReplacementPoliciesID policy) {
    if (associativity == 0) return NULL;
    if (!checkIfPowerOfTwo(numSets) || !checkIfPowerOfTwo(lineSize)) {
        SINUCA3_ERROR_PRINTF(
            "CacheMemory: Trying to get a log2 of a non power of two number\n");
        return NULL;
    }

    unsigned int indexBits = getLog2(numSets);
    unsigned int offsetBits = getLog2(lineSize);
    return CacheMemory::Alocate(indexBits, offsetBits, associativity, policy);
}

CacheMemory *CacheMemory::fromBits(unsigned int numIndexBits,
                                   unsigned int numOffsetBits,
                                   unsigned int associativity,
                                   CacheMemoryNS::ReplacementPoliciesID policy) {
    return CacheMemory::Alocate(numIndexBits, numOffsetBits, associativity,
                                policy);
}

CacheMemory *CacheMemory::Alocate(unsigned int numIndexBits,
                                  unsigned int numOffsetBits,
                                  unsigned int associativity,
                                  CacheMemoryNS::ReplacementPoliciesID policy) {
    if (associativity == 0) return NULL;

    unsigned int numSets = 1u << numIndexBits;

    // Safe

    CacheMemory *cm = new CacheMemory();
    cm->numWays = associativity;
    cm->numSets = numSets;

    cm->offsetBits = numOffsetBits;
    cm->indexBits = numIndexBits;
    cm->tagBits = addrSizeBits - (cm->indexBits + cm->offsetBits);

    cm->offsetMask = (1UL << cm->offsetBits) - 1;
    cm->indexMask = ((1UL << cm->indexBits) - 1) << cm->offsetBits;
    cm->tagMask = ((1UL << cm->tagBits) - 1)
                  << (cm->offsetBits + cm->indexBits);

    size_t n = cm->numSets * cm->numWays;
    cm->entries = new CacheLine *[cm->numSets];
    cm->entries[0] = new CacheLine[n];
    memset(cm->entries[0], 0, n * sizeof(CacheLine));
    for (int i = 1; i < cm->numSets; ++i) {
        cm->entries[i] = cm->entries[0] + (i * cm->numWays);
    }

    for (int i = 0; i < cm->numSets; ++i) {
        for (int j = 0; j < cm->numWays; ++j) {
            cm->entries[i][j].i = i;
            cm->entries[i][j].j = j;
        }
    }

    cm->SetReplacementPolicy(policy);

    return cm;
}

CacheMemory::~CacheMemory() {
    if (this->entries) {
        delete[] this->entries[0];
        delete[] this->entries;
    }
    delete this->policy;
}

bool CacheMemory::Read(MemoryPacket addr) {
    bool exist;
    CacheLine *result;

    exist = this->GetEntry(addr, &result);
    if (exist) {
        this->policy->Acess(result);
    }

    this->statAcess += 1;
    if (exist)
        this->statHit += 1;
    else
        this->statMiss += 1;

    return exist;
}

void CacheMemory::Write(MemoryPacket addr) {
    CacheLine *victim = NULL;
    unsigned long tag = GetTag(addr);
    unsigned long index = GetIndex(addr);

    if (!FindEmptyEntry(addr, &victim)) {
        int set, way;
        this->policy->SelectVictim(tag, index, &set, &way);
        victim = &this->entries[set][way];
    }

    *victim = CacheLine(victim, tag, index);
    this->policy->Acess(victim);
    this->statEvaction += 1;
    return;
}

unsigned long CacheMemory::GetOffset(unsigned long addr) const {
    return addr & this->offsetMask;
}

unsigned long CacheMemory::GetIndex(unsigned long addr) const {
    return (addr & this->indexMask) >> this->offsetBits;
}

unsigned long CacheMemory::GetTag(unsigned long addr) const {
    return (addr & this->tagMask) >> (this->offsetBits + this->indexBits);
}

bool CacheMemory::GetEntry(unsigned long addr, CacheLine **result) const {
    unsigned long tag = this->GetTag(addr);
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine *entry = &this->entries[index][way];

        if (entry->isValid && entry->tag == tag) {
            *result = entry;
            return true;
        }
    }
    return false;
}

bool CacheMemory::FindEmptyEntry(unsigned long addr, CacheLine **result) const {
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine *entry = &this->entries[index][way];

        if (!entry->isValid) {
            *result = entry;
            return true;
        }
    }
    return false;
}

void CacheMemory::SetReplacementPolicy(ReplacementPoliciesID id) {
    switch (id) {
        case LruID:
            this->policy =
                new ReplacementPolicies::LRU(this->numSets, this->numWays);
            return;

        case RandomID:
            this->policy =
                new ReplacementPolicies::Random(this->numSets, this->numWays);
            return;

        case RoundRobinID:
            this->policy = new ReplacementPolicies::RoundRobin(this->numSets,
                                                               this->numWays);
            return;

        default:
            return;
    }
}

void CacheMemory::resetStatistics() {
    this->statMiss = 0;
    this->statHit = 0;
    this->statAcess = 0;
    this->statEvaction = 0;
}

unsigned long CacheMemory::getStatMiss() const { return this->statMiss; }

unsigned long CacheMemory::getStatHit() const { return this->statHit; }

unsigned long CacheMemory::getStatAcess() const { return this->statAcess; }

unsigned long CacheMemory::getStatEvaction() const {
    return this->statEvaction;
}

float CacheMemory::getStatValidProp() const {
    int n = this->numSets * this->numWays;
    int count = 0;
    for (int i = 0; i < n; ++i) {
        if (this->entries[0][i].isValid) count += 1;
    }
    return (float)count / (float)n;
}
