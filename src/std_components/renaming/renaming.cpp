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

#include "renaming.hpp"

int Renaming::Configure(Config config) {
    if (config.ComponentReference("BoomFetcher", &this->fetcher, true))
        return 1;

    if (config.Integer("numIntPhysicalRegisters",
                       (long*)&this->numIntPhysicalRegisters))
        return 1;

    if (this->numIntPhysicalRegisters < 0)
        return config.Error("numIntPhysicalRegisters", "not > 0");

    if (config.Integer("numFpPhysicalRegisters",
                       (long*)&this->numFpPhysicalRegisters))
        return 1;
    if (this->numFpPhysicalRegisters < 0)
        return config.Error("numFpPhysicalRegisters", "not > 0");

    if (config.Integer("robSize", (long*)&this->robSize)) return 1;
    if (this->robSize <= 0) return config.Error("robSize", "not > 0");

    this->numMapRegisters = MAP_MAX_SIZE;
    if (this->numIntPhysicalRegisters == 0)
        this->numIntPhysicalRegisters = DEFAULT_INT_PHYSICAL_REGISTERS;

    if (this->numFpPhysicalRegisters == 0)
        this->numFpPhysicalRegisters = DEFAULT_FP_PHYSICAL_REGISTERS;

    this->totalPhysicalRegisters =
        this->numIntPhysicalRegisters + this->numFpPhysicalRegisters;

    this->fetcherId = this->fetcher->Connect(0);
    if (this->intBusyTable.Allocate(this->numIntPhysicalRegisters)) return 1;
    if (this->fpBusyTable.Allocate(this->numFpPhysicalRegisters)) return 1;
    if (this->intFreeTable.Allocate(this->numIntPhysicalRegisters)) return 1;
    if (this->fpFreeTable.Allocate(this->numFpPhysicalRegisters)) return 1;
    if (this->rob.Allocate(this->robSize)) return 1;

    return 0;
}
