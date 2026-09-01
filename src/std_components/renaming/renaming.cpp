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
    for (unsigned int i = 0; i < this->numIntPhysicalRegisters; ++i)
        this->intFreeTable.SetBin(i);

    for (unsigned int i = 0; i < this->numFpPhysicalRegisters; ++i)
        this->fpFreeTable.SetBin(i);

    for (unsigned int i = 0; i < this->numMapRegisters; ++i) {
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

unsigned int Renaming::MapRegister(Register reg) {
    if (reg.isFloat == false) {
        for (unsigned int i = 1; i < this->numIntPhysicalRegisters; ++i) {
            if (this->intFreeTable.GetBin(i) == 1) {
                this->intFreeTable.ResetBin(i);
                return i;
            }
        }
    } else {
        for (unsigned int i = 1; i < this->numFpPhysicalRegisters; ++i) {
            if (this->fpFreeTable.GetBin(i) == 1) {
                this->fpFreeTable.ResetBin(i);
                return i + this->numIntPhysicalRegisters;
            }
        }
    }

    SINUCA3_ERROR_PRINTF(
        "O NÚMERO DE REGISTRADORES FÍSICOS NÃO É SUFICIENTE\n");

    exit(1);
}

int Renaming::RenameInstruction(const InstructionPacket packet) {
    if (this->rob.IsFull()) return 1;

    Register rd, rsa, rsb;
    rd = packet.staticInfo->writtenRegsArray[0];
    rsa = packet.staticInfo->readRegsArray[0];
    rsb = packet.staticInfo->readRegsArray[1];

    unsigned int spr1, spr2, oldprd, newprd = 0;

    /*
     * We are getting the map of the ISA registers used in this instruction
     */
    if (this->mapTable.find(rsa.val) != this->mapTable.end())
        spr1 = this->mapTable[rsa.val];
    else
        spr1 = MapRegister(rsa);

    if (this->mapTable.find(rsb.val) != this->mapTable.end())
        spr2 = this->mapTable[rsb.val];
    else
        spr2 = MapRegister(rsb);

    if (this->mapTable.find(rd.val) != this->mapTable.end())
        oldprd = this->mapTable[rd.val];
    else
        oldprd = MapRegister(rd);

    /*
     * Instruction only uses integer registers
     */
    if ((rd.isFloat == false) && (rsa.isFloat == false) &&
        (rsb.isFloat == false)) {
        for (unsigned int i = 0; i < this->numIntPhysicalRegisters; ++i) {
            if (this->intFreeTable.GetIterator() != 0 &&
                this->intFreeTable.GetElementIterator() == true) {
                newprd = this->intFreeTable.GetIterator();
                break;
            }

            this->intFreeTable.Next();
        }

        /*
         * There aren't int registers available.
         */
        if (newprd == 0) {
            return 1;
        }

        /*
         * Allocates a new physical register for this instruction.
         * Set the physical register as busy (the value isn't ready) and not
         * free.
         */
        this->intFreeTable.ResetBin(newprd);
        this->intBusyTable.SetBin(newprd);
        this->mapTable[rd.val] = newprd;
        this->rob.Insert(packet, newprd, oldprd, spr1, spr2);
        return 0;
    }

    /*
     * Instruction only uses floating-point registers
     */
    if ((rd.isFloat == true) && (rsa.isFloat == true) &&
        (rsb.isFloat == true)) {
        // Only integer instructions for now

        return 0;
    }

    SINUCA3_DEBUG_PRINTF("A instrução %s utiliza registradores mistos\n",
                         packet.StaticInstructionInfo.instMnemonic);

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

void Renaming::PrintStatistics() {}
