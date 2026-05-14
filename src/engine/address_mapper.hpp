#ifndef SINUCA3_ADDRESS_MAPPER_HPP_
#define SINUCA3_ADDRESS_MAPPER_HPP_

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
 * @file address_mapper.hpp
 * @brief Component responsible for mapping addresses in the simulation. It can
 * be used to implement different address mapping strategies, such as context
 * insertion or page/frame mapping. By default, it uses context insertion, which
 * adds the context ID to the upper bits of the address. It is a singleton
 * component, so it can be accessed from anywhere in the simulation using the
 * alias "ADDRESS_MAPPER".
 */

#include "config/config.hpp"
#include "default_packets.hpp"
#include "engine/component.hpp"

class AddressMapper : public Component<MemoryPacket*> {
  private:
    AddressMapper() : pageFrameMappingEnabled(false) {}

    bool pageFrameMappingEnabled;

    /** @brief Gets the mapping for a given address using context insertion */
    bool GetMappingByContextInsertion(unsigned long address, int context,
                                      unsigned long* mappedAddress);

    /** @brief Gets the mapping for a given address using page/frame mapping */
    bool GetMappingByPageFrame(unsigned long address, int context,
                               unsigned long* mappedAddress);

    /** @brief Gets the address mapping based on the enabled mapping strategy */
    bool GetAddressMapping(unsigned long address, int context,
                           unsigned long* mappedAddress);

  public:
    static AddressMapper* GetInstance() {
        static AddressMapper instance;
        return &instance;
    }
    virtual int Configure(Config config) {
        (void)config;
        return 0;
    }
    virtual void Clock();
    virtual void PrintStatistics() {}
    virtual ~AddressMapper() {}
};

#endif  // SINUCA3_ADDRESS_MAPPER_HPP_
