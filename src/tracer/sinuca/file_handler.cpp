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
 * @file file_handler.cpp
 */

#include "file_handler.hpp"
#include "utils/logger.hpp"

const char* BIN_EXT = ".bin";
const char* TID_FORMAT = "_tid";
const char* TID_MAX = "4294967295";

const int BIN_EXT_SIZE = strlen(BIN_EXT);
const int TID_FORMAT_SIZE = strlen(TID_FORMAT);
const int TID_CHARS = strlen(TID_MAX);

long GetPathTidInSize(const char* dir, const char* pref) {
    long dirLen = (long)strlen(dir);
    long prefLen = (long)strlen(pref);
    return TID_CHARS + TID_FORMAT_SIZE + dirLen + prefLen;
}

void FormatPathTidIn(char* dest, const char* dir, const char* pref, int tid, long destSize) {
    snprintf(dest, destSize, "%s/%s%s%u%s", dir, pref, TID_FORMAT, tid, BIN_EXT);
}

long GetPathTidOutSize(const char* dir, const char* pref) {
    long dirLen = (long)strlen(dir);
    long prefLen = (long)strlen(pref);
    return BIN_EXT_SIZE + dirLen + prefLen;
}

void FormatPathTidOut(char* dest, const char* dir, const char* pref, long destSize) {
    snprintf(dest, destSize, "%s/%s%s", dir, pref, BIN_EXT);
}

int FileHeader::ReserveHeaderSpace(FILE* file) {
    if (file == NULL) {
        return 1;
    }
    if (fseek(file, sizeof(FileHeader), SEEK_SET) != 0) {
        return 1;
    }
    return 0;
}

void FileHeader::SetStaticHeader(int32_t inst, int32_t bbls, int32_t threads) {;
    this->staticHeader.recordedInstructions = inst;
    this->staticHeader.recordedBasicBlocks = bbls;
    this->staticHeader.registeredThreads = threads;
}

void FileHeader::SetDynamicHeader(int64_t executedInstructions) {
    this->dynamicHeader.executedInstructions = executedInstructions;
}

int FileHeader::GetRecordedBasicBlocks() {
    int recordedBbls = 0;
    if (this->IsValid()) {
        recordedBbls = this->staticHeader.recordedBasicBlocks;
    } else {
        SINUCA3_ERROR_PRINTF("Invalid file header!\n");
    }
    return recordedBbls;
}

int FileHeader::GetRecordedInstructions() {
    int recordedInst = 0;
    if (this->IsValid()) {
        recordedInst = this->staticHeader.recordedInstructions;
    } else {
        SINUCA3_ERROR_PRINTF("Invalid file header!\n");
    }
    return recordedInst;
}

int FileHeader::GetRegisteredThreads() {
    int registeredThreads = 0;
    if (this->IsValid()) {
        registeredThreads = this->staticHeader.registeredThreads;
    } else {
        SINUCA3_ERROR_PRINTF("Invalid file header!\n");
    }
    return registeredThreads;
}

long FileHeader::GetExecutedInstructions() {
    long executedInst = 0;
    if (this->IsValid()) {
        executedInst = this->dynamicHeader.executedInstructions;
    } else {
        SINUCA3_ERROR_PRINTF("Invalid file header!\n");
    }
    return executedInst;
}

void FileHeader::PrintVersionAndTarget() const {
    const char* targetStr;
    switch (this->targetArch) {
        case TargetArchX86:
            targetStr = "x86";
            break;
        case TargetArchARM:
            targetStr = "ARM";
            break;
        case TargetArchRISCV:
            targetStr = "RISC-V";
            break;
        default:
            targetStr = "Unknown";
    }
    SINUCA3_WARNING_PRINTF("\t Version: %d\n", this->traceVersion);
    SINUCA3_WARNING_PRINTF("\t Target: %s\n", targetStr);
}