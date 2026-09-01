//
// Copyright (C) 2026 HiPES - Universidade Federal do Paraná
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
 * @file intrinsics.cpp
 */

#include "intrinsics.hpp"

KNOB<std::string> KnobIntrinsics(KNOB_MODE_APPEND, "pintool", "i", "",
                                 "Intrinsic instructions in the format "
                                 "name:readregs:writeregs");

inline int SeparateStringInSections(char* str, char separator, char** sections,
                                    int numberOfSections) {
    if (str[0] == '\0') return 0;
    unsigned int index = 0;
    for (int i = 0; i < numberOfSections; ++i) {
        sections[i] = &str[index];
        while (str[index] != separator) {
            if (str[index] == '\0') return i + 1;
            ++index;
        }
        str[index] = '\0';
        ++index;
    }

    return numberOfSections;
}

inline REG RegisterNameToREG(const char* name) {
    // Every register we actually support.
    if (strcmp(name, "rax") == 0) return LEVEL_BASE::REG_RAX;
    if (strcmp(name, "rbx") == 0) return LEVEL_BASE::REG_RBX;
    if (strcmp(name, "rcx") == 0) return LEVEL_BASE::REG_RCX;
    if (strcmp(name, "rdx") == 0) return LEVEL_BASE::REG_RDX;
    if (strcmp(name, "rsi") == 0) return LEVEL_BASE::REG_RSI;
    if (strcmp(name, "rdi") == 0) return LEVEL_BASE::REG_RDI;
    if (strcmp(name, "rsp") == 0) return LEVEL_BASE::REG_RSP;
    if (strcmp(name, "rbp") == 0) return LEVEL_BASE::REG_RBP;
    if (strcmp(name, "r8") == 0) return LEVEL_BASE::REG_R8;
    if (strcmp(name, "r9") == 0) return LEVEL_BASE::REG_R9;
    if (strcmp(name, "r10") == 0) return LEVEL_BASE::REG_R10;
    if (strcmp(name, "r11") == 0) return LEVEL_BASE::REG_R11;
    if (strcmp(name, "r12") == 0) return LEVEL_BASE::REG_R12;
    if (strcmp(name, "r13") == 0) return LEVEL_BASE::REG_R13;
    if (strcmp(name, "r14") == 0) return LEVEL_BASE::REG_R14;
    if (strcmp(name, "r15") == 0) return LEVEL_BASE::REG_R15;
    if (strcmp(name, "xmm0") == 0) return LEVEL_BASE::REG_XMM0;
    if (strcmp(name, "xmm1") == 0) return LEVEL_BASE::REG_XMM1;
    if (strcmp(name, "xmm2") == 0) return LEVEL_BASE::REG_XMM2;
    if (strcmp(name, "xmm3") == 0) return LEVEL_BASE::REG_XMM3;
    if (strcmp(name, "xmm4") == 0) return LEVEL_BASE::REG_XMM4;
    if (strcmp(name, "xmm5") == 0) return LEVEL_BASE::REG_XMM5;
    if (strcmp(name, "xmm6") == 0) return LEVEL_BASE::REG_XMM6;
    if (strcmp(name, "xmm7") == 0) return LEVEL_BASE::REG_XMM7;
    if (strcmp(name, "xmm8") == 0) return LEVEL_BASE::REG_XMM8;
    if (strcmp(name, "xmm9") == 0) return LEVEL_BASE::REG_XMM9;
    if (strcmp(name, "xmm10") == 0) return LEVEL_BASE::REG_XMM10;
    if (strcmp(name, "xmm11") == 0) return LEVEL_BASE::REG_XMM11;
    if (strcmp(name, "xmm12") == 0) return LEVEL_BASE::REG_XMM12;
    if (strcmp(name, "xmm13") == 0) return LEVEL_BASE::REG_XMM13;
    if (strcmp(name, "xmm14") == 0) return LEVEL_BASE::REG_XMM14;
    if (strcmp(name, "xmm15") == 0) return LEVEL_BASE::REG_XMM15;
    if (strcmp(name, "ymm0") == 0) return LEVEL_BASE::REG_YMM0;
    if (strcmp(name, "ymm1") == 0) return LEVEL_BASE::REG_YMM1;
    if (strcmp(name, "ymm2") == 0) return LEVEL_BASE::REG_YMM2;
    if (strcmp(name, "ymm3") == 0) return LEVEL_BASE::REG_YMM3;
    if (strcmp(name, "ymm4") == 0) return LEVEL_BASE::REG_YMM4;
    if (strcmp(name, "ymm5") == 0) return LEVEL_BASE::REG_YMM5;
    if (strcmp(name, "ymm6") == 0) return LEVEL_BASE::REG_YMM6;
    if (strcmp(name, "ymm7") == 0) return LEVEL_BASE::REG_YMM7;
    if (strcmp(name, "ymm8") == 0) return LEVEL_BASE::REG_YMM8;
    if (strcmp(name, "ymm9") == 0) return LEVEL_BASE::REG_YMM9;
    if (strcmp(name, "ymm10") == 0) return LEVEL_BASE::REG_YMM10;
    if (strcmp(name, "ymm11") == 0) return LEVEL_BASE::REG_YMM11;
    if (strcmp(name, "ymm12") == 0) return LEVEL_BASE::REG_YMM12;
    if (strcmp(name, "ymm13") == 0) return LEVEL_BASE::REG_YMM13;
    if (strcmp(name, "ymm14") == 0) return LEVEL_BASE::REG_YMM14;
    if (strcmp(name, "ymm15") == 0) return LEVEL_BASE::REG_YMM15;

    return LEVEL_BASE::REG_INVALID();
}

inline void SetRegistersInIntrinsicsInfo(REG* arr, unsigned char* num,
                                         char* str) {
    char* sections[MAX_REGISTERS];
    *num = SeparateStringInSections(str, ',', sections, MAX_REGISTERS);
    for (unsigned char i = 0; i < *num; ++i) {
        arr[i] = RegisterNameToREG(sections[i]);
    }
}

void LoadIntrinsics() {
    // This also fails silently. TODO: Make all of this better and crash with a
    // good error message when needed.
    for (unsigned int knobIdx = 0; knobIdx < KnobIntrinsics.NumberOfValues();
         ++knobIdx) {
        std::string value = KnobIntrinsics.Value(knobIdx);
        char* strValue = (char*)alloca(sizeof(char) * (value.size() + 1));
        memcpy((void*)strValue, (void*)value.c_str(),
               sizeof(char) * (value.size() + 1));

        char* sections[3];
        SeparateStringInSections(strValue, ':', sections, 3);
        char* name = sections[0];
        char* readRegs = sections[1];
        char* writeRegs = sections[2];

        SINUCA3_LOG_PRINTF("Using intrinsic: %s readRegs: %s writeRegs: %s\n",
                           name, readRegs, writeRegs);

        intrinsics.push_back(IntrinsicInfo{});
        IntrinsicInfo* i = &intrinsics[intrinsics.size() - 1];

        // Copy the name.
        unsigned int nameSize = strlen(name);
        if (nameSize > INST_MNEMONIC_LEN) nameSize = INST_MNEMONIC_LEN - 1;
        memcpy(&i->name[0], name, nameSize + 1);
        strcpy(&i->loaderName[0], "__");
        strcat(&i->loaderName[0], &i->name[0]);
        strcat(&i->loaderName[0], "Loader");

        // Copy registers.
        SetRegistersInIntrinsicsInfo(i->read, &i->numReadRegs, readRegs);
        SetRegistersInIntrinsicsInfo(i->write, &i->numWriteRegs, writeRegs);
    }
}

IntrinsicInfo* GetIntrinsicInfo(const INS* ins) {
    if (!INS_IsDirectControlFlow(*ins)) {
        return NULL;
    }

    ADDRINT targetAddr = INS_DirectControlFlowTargetAddress(*ins);
    RTN targetRtn = RTN_FindByAddress(targetAddr);
    if (RTN_Valid(targetRtn)) {
        const char* targetName = RTN_Name(targetRtn).c_str();
        for (unsigned int i = 0; i < intrinsics.size(); ++i) {
            if (strcmp(targetName, intrinsics[i].loaderName) == 0)
                return &intrinsics[i];
        }
    }

    return NULL;
}

int IntrinsicToSinucaInst(const INS* originalCall, IntrinsicInfo* info,
                          CompressedInstruction* inst) {
    memset(inst, 0, sizeof(*inst));

    unsigned long size = sizeof(inst->instructionMnemonic) - 1;
    strncpy(inst->instructionMnemonic, info->name, size);
    if (size < strlen(info->name)) {
        SINUCA3_WARNING_PRINTF("Insufficient space to store mnemonic\n");
    }

    inst->instructionAddress = INS_Address(*originalCall);
    inst->instructionSize = INS_Size(*originalCall);
    inst->readRegs.occupation = info->numReadRegs;
    inst->writtenRegs.occupation = info->numWriteRegs;

    // outros campos nao preenchidos

    const unsigned long readRegsArraySize =
        sizeof(inst->readRegs.regs) / sizeof(*inst->readRegs.regs);
    if (readRegsArraySize < info->numReadRegs) {
        SINUCA3_WARNING_PRINTF("Insufficient space to store read regs\n");
        return 1;
    }
    for (unsigned long i = 0; i < readRegsArraySize; i++) {
        inst->readRegs.regs[i].val = info->read[i];
        inst->readRegs.regs[i].isFp = 0;
    }

    const unsigned long writeRegsArraySize =
        sizeof(inst->writtenRegs.regs) / sizeof(*inst->writtenRegs.regs);
    if (writeRegsArraySize < info->numWriteRegs) {
        SINUCA3_WARNING_PRINTF("Insufficient space to store write regs\n");
        return 1;
    }
    for (unsigned long i = 0; i < writeRegsArraySize; i++) {
        inst->writtenRegs.regs[i].val = info->write[i];
        inst->writtenRegs.regs[i].isFp = 0;
    }

    return 0;
}
