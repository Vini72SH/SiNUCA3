#ifndef SINUCA3_INTRINSICS_HPP_
#define SINUCA3_INTRINSICS_HPP_

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
 * @file intrinsics.hpp
 */

#include <tracer/sinuca/file_handler.hpp>
#include <vector>

#include "pin.H"

extern KNOB<std::string> KnobIntrinsics;

struct IntrinsicInfo {
    char name[INST_MNEMONIC_LEN - 1];
    char loaderName[INST_MNEMONIC_LEN + sizeof("__Loader")];  // Cache.
    REG read[MAX_REGISTERS];
    REG write[MAX_REGISTERS];
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
};

extern std::vector<IntrinsicInfo> intrinsics;

/** @brief Separates a string into sections based on a separator. */
inline int SeparateStringInSections(char* str, char separator, char** sections,
                                    int numberOfSections);

/** @brief Converts a register name to a REG enum value. */
inline REG RegisterNameToREG(const char* name);

/** @brief Sets the registers in the intrinsic info based on a string. */
inline void SetRegistersInIntrinsicsInfo(REG* arr, unsigned char* num,
                                         char* str);

/** @brief Loads the intrinsic instructions from the command line. */
void LoadIntrinsics();

/** @brief Gets the intrinsic info for a given instruction. */
IntrinsicInfo* GetIntrinsicInfo(const INS* ins);

/** @brief Converts an intrinsic instruction to a SiNUCA instruction. */
int IntrinsicToSinucaInst(const INS* originalCall, IntrinsicInfo* info,
                          CompressedInstruction* inst);

#endif  // SINUCA3_INTRINSICS_HPP_