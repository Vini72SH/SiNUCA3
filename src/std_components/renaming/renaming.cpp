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

    this->fetcherID = this->fetcher->Connect(0);
    if (this->intBusyTable.Allocate(this->numIntPhysicalRegisters)) return 1;
    if (this->fpBusyTable.Allocate(this->numFpPhysicalRegisters)) return 1;
    if (this->intFreeTable.Allocate(this->numIntPhysicalRegisters)) return 1;
    if (this->fpFreeTable.Allocate(this->numFpPhysicalRegisters)) return 1;
    if (this->rob.Allocate(this->robSize)) return 1;

    this->packets.Allocate(0, sizeof(struct RenamingPacket));

    /*
     * Sets all registers with free
     */
    for (int i = 0; i < this->numIntPhysicalRegisters; ++i)
        this->intFreeTable.SetBin(i);

    for (int i = 0; i < this->numFpPhysicalRegisters; ++i)
        this->fpFreeTable.SetBin(i);

    for (int i = 0; i < this->numMapRegisters; ++i) {
        this->mapTable[i] = (i + 1) % this->numIntPhysicalRegisters;
        this->intFreeTable.ResetBin((i + 1) % this->numIntPhysicalRegisters);
    }
    /*
     * The physical register 0 is reserved for ZERO.
     */
    this->intFreeTable.ResetBin(0);
    this->fpFreeTable.ResetBin(0);

    return 0;
}

int Renaming::RenameInstruction(const InstructionPacket packet) {
    if (this->rob.IsFull()) return 1;

    /*
     * Instruction uses int registers
     */
    if (1) {
        int intPhysicalReg1 =
            this->mapTable[packet.staticInfo->readRegsArray[0]];
        int intPhysicalReg2 =
            this->mapTable[packet.staticInfo->readRegsArray[1]];

        int intOldPRD = this->mapTable[packet.staticInfo->writtenRegsArray[0]];
        int intNewPRD = -1;

        for (int i = 0; i < this->numIntPhysicalRegisters; ++i) {
            if (this->intFreeTable.GetElementIterator() == true) {
                intNewPRD = this->intFreeTable.GetIterator();
                break;
            }

            this->intFreeTable.Next();
        }

        /*
         * There aren't int registers available.
         */
        if (intNewPRD == -1) {
            return 1;
        }

        /*
         * Allocates a new physical register for this instruction.
         * Set the physical register as busy (the value isn't ready) and not
         * free.
         */
        this->intFreeTable.ResetBin(intNewPRD);
        this->intBusyTable.SetBin(intNewPRD);
        this->mapTable[packet.staticInfo->writtenRegsArray[0]] = intNewPRD;

        this->rob.Insert(packet, intNewPRD, intOldPRD, intPhysicalReg1,
                         intPhysicalReg2);
    } else {
        int fpPhysicalReg1 =
            this->mapTable[packet.staticInfo->readRegsArray[0]] -
            this->numIntPhysicalRegisters;
        int fpPhysicalReg2 =
            this->mapTable[packet.staticInfo->readRegsArray[1]] -
            this->numIntPhysicalRegisters;

        int fpOldPRD = this->mapTable[packet.staticInfo->writtenRegsArray[0]] -
                       this->numIntPhysicalRegisters;
        int fpNewPRD = -1;

        for (int i = 0; i < this->numFpPhysicalRegisters; ++i) {
            if (this->fpFreeTable.GetElementIterator() == true) {
                fpNewPRD = this->fpFreeTable.GetIterator();
                break;

                this->fpFreeTable.Next();
            }
        }

        if (fpNewPRD == -1) {
            return 1;
        }

        this->fpFreeTable.ResetBin(fpNewPRD);
        this->fpFreeBusyTable.SetBin(fpNewPRD);
        this->mapTable[packet.staticInfo->writtenRegsArray[0]] =
            fpNewPRD + this->numIntPhysicalRegisters;

        this->rob.Insert(packet, fpNewPRD, fpOldPRD, fpPhysicalReg1,
                         fpPhysicalReg2);
    }

    return 0;
}

void Renaming::PacketBuffering() {
    RenamingPacket packet;
    long numberOfConnections = this->GetNumberOfConnections();

    for (long i = 0; i < numberOfConnections; ++i) {
        while (this->ReceiveResponse(i, &packet) == 0)
            this->packets.Enqueue(&packet);
    }
}

void Renaming::PacketHandler() {
    RenamingPacket packet;
    bool running = true;

    while (!this->packets.IsEmpty() && running) {
        this->packets.GetFirstElement(&packet);

        switch (packet.type) {
            case RENAME:
                /*
                 * Check if was possible to allocate an entry in rob or exists
                 * free registers.
                 */
                if (!(this->RenameInstruction(packet.data.instruction))) {
                    this->packets.Pop();

                } else {
                    // Should sent a stall message for the fetcher.
                    running = false;
                }

                break;

            default:
                break;
        }
    }
}

void Renaming::Clock() {
    PacketBuffering();
    PacketHandler();
}
