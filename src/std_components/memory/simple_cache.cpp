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
 * @file simple_cache.cpp
 * @brief Implementation of the SimpleCache.
 */

#include "simple_cache.hpp"

#include <sinuca3.hpp>

#include "config/config.hpp"
#include "utils/logging.hpp"

int SimpleCache::FinishSetup() {
    if (this->cacheSize == 0) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"cacheSize\"\n");
        return 1;
    }

    if (this->lineSize == 0) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"lineSize\"\n");
        return 1;
    }

    if (this->numWays == 0) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"associativity\"\n");
        return 1;
    }

    if (this->policyID == CacheMemoryNS::Unset) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"policy\"\n");
        return 1;
    }

    this->cache = CacheMemory<unsigned long>::fromCacheSize(this->cacheSize, this->lineSize,
                                             this->numWays, this->policyID);
    if (this->cache == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alocate CacheMemory\n");
        return 1;
    }

    return 0;
}

int SimpleCache::ConfigCacheSize(ConfigValue value) {
    if (value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF(
            "Cache parameter \"cacheSize\" is not an integer.\n");
        return 1;
    }

    const long v = value.value.integer;
    if (v <= 0) {
        SINUCA3_ERROR_PRINTF(
            "Invalid value for Cache parameter \"cacheSize\": should be > "
            "0.");
        return 1;
    }
    this->cacheSize = v;
    return 0;
}

int SimpleCache::ConfigLineSize(ConfigValue value) {
    if (value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF(
            "Cache parameter \"lineSize\" is not an integer.\n");
        return 1;
    }

    const long v = value.value.integer;
    if (v <= 0) {
        SINUCA3_ERROR_PRINTF(
            "Invalid value for Cache parameter \"lineSize\": should be > "
            "0.");
        return 1;
    }
    this->lineSize = v;
    return 0;
}

int SimpleCache::ConfigAssociativity(ConfigValue value) {
    if (value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF(
            "Cache parameter \"associativity\" is not an integer.\n");
        return 1;
    }

    const long v = value.value.integer;
    if (v <= 0) {
        SINUCA3_ERROR_PRINTF(
            "Invalid value for Cache parameter \"associativity\": should "
            "be > 0.");
        return 1;
    }
    this->numWays = v;
    return 0;
}

int SimpleCache::ConfigPolicy(ConfigValue value) {
    if (value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF("Cache parameter \"policy\" is not an integer.\n");
        return 1;
    }

    const CacheMemoryNS::ReplacementPoliciesID v =
        static_cast<CacheMemoryNS::ReplacementPoliciesID>(value.value.integer);
    this->policyID = v;
    return 0;
}

int SimpleCache::SetConfigParameter(const char* parameter, ConfigValue value) {
    if (strcmp(parameter, "cacheSize") == 0) {
        return this->ConfigCacheSize(value);
    }

    if (strcmp(parameter, "lineSize") == 0) {
        return this->ConfigLineSize(value);
    }

    if (strcmp(parameter, "associativity") == 0) {
        return this->ConfigAssociativity(value);
    }

    if (strcmp(parameter, "policy") == 0) {
        return this->ConfigPolicy(value);
    }

    SINUCA3_ERROR_PRINTF("Cache received an unkown parameter: %s.\n",
                         parameter);
    return 1;
}

void SimpleCache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: SimpleCache Clock!\n", this);
    long numberOfConnections = this->GetNumberOfConnections();
    MemoryPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            ++this->numberOfRequests;

            // We dont have (and dont need) data to send back, so a
            // MemoryPacket is send back to to signal
            // that the cache's operation has been completed.

            SINUCA3_DEBUG_PRINTF("%p: SimpleCache Message (%lu) Received!\n",
                                 this, packet);

            // Read() returns NULL if it was a miss.
            if (this->cache->Read(packet)) {
                SINUCA3_DEBUG_PRINTF("%p: SimpleCache HIT!\n", this);
            } else {
                SINUCA3_DEBUG_PRINTF("%p: SimpleCache MISS!\n", this);
                this->cache->Write(packet, &packet);
            }

            this->SendResponseToConnection(i, &packet);
        }
    }
}

void SimpleCache::PrintStatistics() {
    SINUCA3_DEBUG_PRINTF(
        "%p: SimpleCache Stats:\n\tMiss: %lu\n\tHit: %lu\n\tAcces: "
        "%lu\n\tEvaction: %lu\n\tValidProp: %.3f\n",
        this, this->cache->getStatMiss(), this->cache->getStatHit(),
        this->cache->getStatAcess(), this->cache->getStatEvaction(),
        this->cache->getStatValidProp());
}
