#ifndef SINUCA3_CACHE_MEMORY_T_HPP_
#define SINUCA3_CACHE_MEMORY_T_HPP_

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
 * @file cacheMemory.t.hpp
 * @brief Implementation of cacheMemory.
 */

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <utils/logging.hpp>

// Including the header file here is NOT necessary for compilation,
// because this file is always included at the end of the header.
// However, some text editors can use it to correctly locate symbols
// for code navigation, autocomplete, and diagnostics.
#include "cacheMemory.hpp"
#include "replacement_policy.hpp"
#include "utils/cache/replacement_policies/lru.hpp"
#include "utils/cache/replacement_policies/random.hpp"
#include "utils/cache/replacement_policies/roundRobin.hpp"

/**
 * @brief Number of bits in a address.
 * Default is 64 bits.
 * Used to calculate how many bits are used for tag.
 * If you want to change this, may be better to clone this file
 * and create another CacheMemory class for this.
 */
static const int addrSizeBits = 64;

static inline double getLog2(double x) { return log(x) / log(2); }

static inline bool checkIfPowerOfTwo(unsigned long x) {
    if (x == 0) return false;  // 0 is not a power of 2
    // x is power of 2 if only one bit is set.
    return (x & (x - 1)) == 0;
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::fromCacheSize(
    unsigned int cacheSize, unsigned int lineSize, unsigned int associativity,
    const char* policy) {
    unsigned int numSets = cacheSize / (lineSize * associativity);
    return CacheMemory<ValueType>::fromNumSets(numSets, lineSize, associativity,
                                               policy);
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::fromNumSets(
    unsigned int numSets, unsigned int lineSize, unsigned int associativity,
    const char* policy) {
    if (associativity == 0) return NULL;
    if (!checkIfPowerOfTwo(numSets)) {
        SINUCA3_ERROR_PRINTF(
            "CacheMemory: numSets cannot be %u because it is not power of "
            "two.\n",
            numSets);
    }
    if (!checkIfPowerOfTwo(lineSize)) {
        SINUCA3_ERROR_PRINTF(
            "CacheMemory: lineSize cannot be %u because it is not power of "
            "two.\n",
            lineSize);
        return NULL;
    }

    unsigned int indexBits = getLog2(numSets);
    unsigned int offsetBits = getLog2(lineSize);
    return CacheMemory<ValueType>::Alocate(indexBits, offsetBits, associativity,
                                           policy);
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::fromBits(
    unsigned int numIndexBits, unsigned int numOffsetBits,
    unsigned int associativity, const char* policy) {
    return CacheMemory<ValueType>::Alocate(numIndexBits, numOffsetBits,
                                           associativity, policy);
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::Alocate(
    unsigned int numIndexBits, unsigned int numOffsetBits,
    unsigned int associativity, const char* policy) {
    if (associativity == 0) {
        SINUCA3_ERROR_PRINTF(
            "CacheMemory: associativity cannot be equal to 0.\n");
        return NULL;
    }

    unsigned int numSets = 1u << numIndexBits;

    CacheMemory<ValueType>* cm = new CacheMemory<ValueType>();
    cm->numWays = associativity;
    cm->numSets = numSets;

    cm->offsetBits = numOffsetBits;
    cm->indexBits = numIndexBits;
    cm->tagBits = addrSizeBits - (cm->indexBits + cm->offsetBits);

    cm->offsetMask = (1UL << cm->offsetBits) - 1;
    cm->indexMask = ((1UL << cm->indexBits) - 1) << cm->offsetBits;
    cm->tagMask = ((1UL << cm->tagBits) - 1)
                  << (cm->offsetBits + cm->indexBits);

    if (cm->SetReplacementPolicy(policy)) {
        delete cm;
        SINUCA3_ERROR_PRINTF("CacheMemory: Invalid replacement policy.\n");
        return NULL;
    }

    size_t n = cm->numSets * cm->numWays;
    cm->entries = new CacheLine*[cm->numSets];
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

    cm->data = new ValueType*[cm->numSets];
    cm->data[0] = new ValueType[n];
    memset(cm->data[0], 0, n * sizeof(ValueType));
    for (int i = 1; i < cm->numSets; ++i) {
        cm->data[i] = cm->data[0] + (i * cm->numWays);
    }

    return cm;
}

template <typename ValueType>
CacheMemory<ValueType>::~CacheMemory() {
    if (this->entries) {
        delete[] this->entries[0];
        delete[] this->entries;
    }

    if (this->data) {
        delete[] this->data[0];
        delete[] this->data;
    }

    delete this->policy;
}

template <typename ValueType>
const ValueType* CacheMemory<ValueType>::Read(unsigned long addr) {
    bool exist;
    CacheLine* line;
    const ValueType* result = NULL;

    exist = this->GetEntry(addr, &line);
    if (exist) {
        this->policy->Acess(line);
        result = static_cast<const ValueType*>(&this->data[line->i][line->j]);
    }

    this->statAcess += 1;
    if (exist)
        this->statHit += 1;
    else
        this->statMiss += 1;

    return result;
}

template <typename ValueType>
void CacheMemory<ValueType>::Write(unsigned long addr, const ValueType* data) {
    CacheLine* victim = NULL;
    unsigned long tag = GetTag(addr);
    unsigned long index = GetIndex(addr);

    if (!FindEmptyEntry(addr, &victim)) {
        int set, way;
        this->policy->SelectVictim(tag, index, &set, &way);
        victim = &this->entries[set][way];
    }

    *victim = CacheLine(victim, tag, index);
    this->policy->Acess(victim);

    this->data[victim->i][victim->j] = *data;

    this->statEvaction += 1;
    return;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::GetOffset(unsigned long addr) const {
    return addr & this->offsetMask;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::GetIndex(unsigned long addr) const {
    return (addr & this->indexMask) >> this->offsetBits;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::GetTag(unsigned long addr) const {
    return (addr & this->tagMask) >> (this->offsetBits + this->indexBits);
}

template <typename ValueType>
bool CacheMemory<ValueType>::GetEntry(unsigned long addr,
                                      CacheLine** result) const {
    unsigned long tag = this->GetTag(addr);
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine* entry = &this->entries[index][way];

        if (entry->isValid && entry->tag == tag) {
            *result = entry;
            return true;
        }
    }
    return false;
}

template <typename ValueType>
bool CacheMemory<ValueType>::FindEmptyEntry(unsigned long addr,
                                            CacheLine** result) const {
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine* entry = &this->entries[index][way];

        if (!entry->isValid) {
            *result = entry;
            return true;
        }
    }
    return false;
}

template <typename ValueType>
int CacheMemory<ValueType>::SetReplacementPolicy(const char* policyName) {
    if (strcmp(policyName, "lru") == 0) {
        this->policy =
            new ReplacementPolicies::LRU(this->numSets, this->numWays);
        return 0;
    }

    if (strcmp(policyName, "random") == 0) {
        this->policy =
            new ReplacementPolicies::Random(this->numSets, this->numWays);
        return 0;
    }

    if (strcmp(policyName, "roundrobin") == 0) {
        this->policy =
            new ReplacementPolicies::RoundRobin(this->numSets, this->numWays);
        return 0;
    }

    return 1;
}

template <typename ValueType>
void CacheMemory<ValueType>::resetStatistics() {
    this->statMiss = 0;
    this->statHit = 0;
    this->statAcess = 0;
    this->statEvaction = 0;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatMiss() const {
    return this->statMiss;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatHit() const {
    return this->statHit;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatAcess() const {
    return this->statAcess;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatEvaction() const {
    return this->statEvaction;
}

template <typename ValueType>
float CacheMemory<ValueType>::getStatValidProp() const {
    int n = this->numSets * this->numWays;
    int count = 0;
    for (int i = 0; i < n; ++i) {
        if (this->entries[0][i].isValid) count += 1;
    }
    return (float)count / (float)n;
}

#endif
