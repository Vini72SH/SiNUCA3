#ifndef SINUCA3_CACHE_MEMORY_HPP_
#define SINUCA3_CACHE_MEMORY_HPP_

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
 * @file cacheMemory.hpp
 * @brief Templated n-way cache memory.
 **/

#include <sinuca3.hpp>

#include "replacement_policy.hpp"

namespace CacheMemoryNS {
enum ReplacementPoliciesID {
    Unset = -1,
    LruID = 0,
    RandomID = 1,
    RoundRobinID = 2
};
}

struct CacheLine {
    unsigned long tag;
    unsigned long index;
    bool isValid;

    // This is the position of this entry in the entries matrix.
    // It can be useful for organizing other matrices.
    int i, j;

    CacheLine() {};

    inline CacheLine(int i, int j, unsigned long tag, unsigned long index)
        : tag(tag), index(index), isValid(false), i(i), j(j) {};

    inline CacheLine(CacheLine *entry, unsigned long tag, unsigned long index)
        : tag(tag), index(index), isValid(true), i(entry->i), j(entry->j) {};
};

/**
 * @breaf CacheMemory is a class that facilitates the creation of components
 * that have memory that behaves like a cache (apart from the coherence
 * protocol).
 *
 * @details
 * The CacheMemory class provides a generic implementation of an N-way
 * set-associative cache.
 *
 * This structure stores data by using part of the address as the set index,
 * allowing fast access to cached entries. Each set can contain multiple ways
 * (lines), as defined by the associativity. In case multiple addresses map to
 * the same set (a collision), a replacement policy is used to decide which
 * entry to evict.
 *
 * It is templated on the type of value stored (ValueType) and supports
 * operations such as:
 *   - Read: Lookup for a entry using a address.
 *   - Write: Store a value in the cache, using a replacement policy if
 * necessary.
 *
 * The class provides multiple static creation methods (fromCacheSize,
 * fromNumSets, fromBits) and any of them can be used. The different
 * constructors exist to make it easier to create a CacheMemory instance
 * depending on the context and the information you have available (total size,
 * number of sets, or number of index/offset bits).
 *
 * @usage
 * Include the header and instantiate with the desired type:
 * @code
 * CacheMemory<int> *myCache;
 * myCache = CacheMemory<int>::fromCacheSize(16384, 64, 4,
 * CacheMemoryNS::LruID); const int* value; if (myCache->Lookup(addr, &value)) {
 *     // cache hit
 * }
 * myCache->Write(addr, &newValue);
 * @endcode
 */
template <typename ValueType>
class CacheMemory {
  public:
    /**
     * @brief Creates a cache based on the total cache size and line size.
     *
     * @param cacheSize Total size of the cache in bytes.
     * @param lineSize Size of a single cache line in bytes. Determines how many
     * bits of the address are used as the offset within a line. Can be 0 if not
     * relevant.
     * @param associativity Number of ways per set (N in N-way set-associative
     * cache). Cannot be 0.
     * @param policy Replacement policy to use when inserting or evicting cache
     * lines.
     *
     * @return Pointer to a newly created CacheMemory instance.
     *
     * @detail
     * Both numSets and lineSize must be powers of two.
     * This ensures that address bits can be cleanly divided
     * into index and offset fields.
     *
     * @note
     * numSets = (cacheSize / (lineSize * associativity))
     */
    static CacheMemory *fromCacheSize(
        unsigned int cacheSize, unsigned int lineSize,
        unsigned int associativity,
        CacheMemoryNS::ReplacementPoliciesID policy);

    /**
     * @brief Creates a cache specifying the number of sets and line size.
     *
     * @param numSets Number of sets in the cache.
     * @param lineSize Size of a single cache line in bytes. Determines how many
     * bits of the address are used as the offset within a line. Can be 0 if not
     * relevant.
     * @param associativity Number of ways per set (N in N-way set-associative
     * cache). Cannot be 0.
     * @param policy Replacement policy to use when inserting or evicting cache
     * lines.
     *
     * @return Pointer to a newly created CacheMemory instance.
     *
     * @detail
     * Both numSets and lineSize must be powers of two.
     * This ensures that address bits can be cleanly divided
     * into index and offset fields.
     */
    static CacheMemory *fromNumSets(
        unsigned int numSets, unsigned int lineSize, unsigned int associativity,
        CacheMemoryNS::ReplacementPoliciesID policy);

    /**
     * @brief Creates a cache specifying the number of index and offset bits
     * directly.
     *
     * @param numIndexBits Number of bits used for the set index.
     * @param numOffsetBits Number of bits used for the offset within a cache
     * line.  Can be 0 if not relevant.
     * @param associativity Number of ways per set (N in N-way set-associative
     * cache). Cannot be 0.
     * @param policy Replacement policy to use when inserting or evicting cache
     * lines.
     *
     * @return Pointer to a newly created CacheMemory instance.
     *
     * @details
     * The number of sets is determined by the number of bits used for the
     * index. The remaining bits of the address not used for index or offset
     * will serve as the tag.
     */
    static CacheMemory *fromBits(unsigned int numIndexBits,
                                 unsigned int numOffsetBits,
                                 unsigned int associativity,
                                 CacheMemoryNS::ReplacementPoliciesID policy);

    virtual ~CacheMemory();

    /**
     * @brief Looks up a cached value for the specified memory address.
     *
     * This method attempts to locate the cache line corresponding to the
     * provided memory address. If the address is found (cache HIT), it returns
     * a pointer directly to the cached value.
     *
     * The returned pointer refers to a constant value stored internally within
     * the cache. This design avoids mandatory data copies during lookup
     * operations, while ensuring that the caller cannot modify the cached
     * content directly.
     *
     * @tparam ValueType Type of the values stored in the cache.
     * @param addr Memory address to look up in the cache.
     * @return Pointer to the cached value if found (HIT), or `NULL` if not
     * found (MISS).
     */
    const ValueType *Read(MemoryPacket addr);

    /**
     * @brief Writes a value into the cache for the specified memory address.
     *
     * The data pointed to by @p result is copied into the internal cache
     * storage.
     *
     * If there is no invalid (empty) cache line available in the corresponding
     * set, a replacement policy must be applied to select which existing entry
     * will be evicted.
     *
     * @tparam ValueType Type of the values stored in the cache.
     * @param addr Memory address to write to.
     * @param[in] result Pointer to the value to be written into the cache.
     */
    void Write(MemoryPacket addr, const ValueType *data);

    unsigned long GetOffset(unsigned long addr) const;
    unsigned long GetIndex(unsigned long addr) const;
    unsigned long GetTag(unsigned long addr) const;

    /**
     * @brief Find the entry for a addr.
     * @param addr Address to look for.
     * @param result Pointer to store search result.
     * @return True if found, false otherwise.
     */
    bool GetEntry(unsigned long addr, CacheLine **result) const;

    /**
     * @brief Can be used to find a entry that is not valid.
     * @param addr Address to look for.
     * @param result Pointer to store result.
     * @return True if victim is found, false otherwise.
     */
    bool FindEmptyEntry(unsigned long addr, CacheLine **result) const;

    void resetStatistics();
    unsigned long getStatMiss() const;
    unsigned long getStatHit() const;
    unsigned long getStatAcess() const;
    unsigned long getStatEvaction() const;
    float getStatValidProp() const;

  protected:
    int numWays;
    int numSets;

    unsigned int offsetBits;
    unsigned int indexBits;
    unsigned int tagBits;
    unsigned long offsetMask;
    unsigned long indexMask;
    unsigned long tagMask;

    CacheLine **entries;  // matrix [sets x ways]

    ValueType **data;  // matrix [sets x ways]

    ReplacementPolicy *policy;

    // Statistics
    unsigned long statMiss;
    unsigned long statHit;
    unsigned long statAcess;
    unsigned long statEvaction;

    inline CacheMemory()
        : statMiss(0), statHit(0), statAcess(0), statEvaction(0) {};

    static CacheMemory *Alocate(unsigned int numIndexBits,
                                unsigned int numOffsetBits,
                                unsigned int associativity,
                                CacheMemoryNS::ReplacementPoliciesID policy);
    void SetReplacementPolicy(CacheMemoryNS::ReplacementPoliciesID id);
};

#include "cacheMemory.t.hpp"

#endif
