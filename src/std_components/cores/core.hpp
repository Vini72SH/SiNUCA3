#ifndef SINUCA3_CORE_HPP_
#define SINUCA3_CORE_HPP_

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
 * @file core.hpp
 * @details // Todo: details
 */

#include <sinuca3.hpp>

class Core : public Component<int> {
  private:
    Component<FetchPacket>* engine;
    Component<FetchPacket>* fetcher;
    Component<MemoryPacket>* memoryManagementUnit;

    int fetcherConnectionID;
    int mmuConnectionID;
    int engineConnectionID;

    int coreId;
    int contextId;

    bool wasCoreSet;
    bool hasRequestedContext;

    int CoreSetup();
    int RequestCoreContext();
    int ReceiveCoreContext();
    int HandEngineConnOwnershipToFetcher();
    int ForwardContextIdentifierToMmu();


  public:
    inline Core() : engine(NULL), fetcher(NULL), memoryManagementUnit(NULL), wasCoreSet(false), hasRequestedContext(false) {
        this->coreId = totalCores++;
    }
    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();
    ~Core() {}
};



#endif // SINUCA3_CORE_HPP_