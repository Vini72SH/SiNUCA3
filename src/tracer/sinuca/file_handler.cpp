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
 * @file file_handler.cpp
 */

#include "file_handler.hpp"

const char* BIN_EXT = ".bin";
const char* TID_SUFFIX = "_tid";
const char* TID_MAX = "4294967295";

const int BIN_EXT_SIZE = strlen(BIN_EXT);
const int TID_SUFFIX_SIZE = strlen(TID_SUFFIX);
const int TID_CHARS = strlen(TID_MAX);

const char* GetFormattedPath(const char* directory, const char* prefix) {
    long required = GetPathTidOutSize(directory, prefix);
    char* formatted = new char[required];
    FormatPathTidOut(formatted, directory, prefix, required);
    return formatted;
}

const char* GetFormattedPath(const char* directory, const char* prefix,
                             int tid) {
    long required = GetPathTidInSize(directory, prefix);
    char* formatted = new char[required];
    FormatPathTidIn(formatted, directory, prefix, tid, required);
    return formatted;
}

long GetPathTidInSize(const char* dir, const char* preffix) {
    long dirLen = (long)strlen(dir);
    long prefLen = (long)strlen(preffix);
    return dirLen + prefLen + TID_SUFFIX_SIZE + TID_CHARS + BIN_EXT_SIZE +
           1;  // +1 for null terminator
}

void FormatPathTidIn(char* dest, const char* dir, const char* pref, int tid,
                     long destSize) {
    snprintf(dest, destSize, "%s%s%s%u%s", dir, pref, TID_SUFFIX, tid, BIN_EXT);
}

long GetPathTidOutSize(const char* dir, const char* preffix) {
    long dirLen = (long)strlen(dir);
    long prefLen = (long)strlen(preffix);
    return dirLen + prefLen + BIN_EXT_SIZE + 1;  // +1 for null terminator
}

void FormatPathTidOut(char* dest, const char* dir, const char* pref,
                      long destSize) {
    snprintf(dest, destSize, "%s%s%s", dir, pref, BIN_EXT);
}

void FileHeader::Print(bool printMetadata) const {
    SINUCA3_LOG_PRINTF("  %-18s: 0x%X\n", "Magic", this->magic);
    SINUCA3_LOG_PRINTF("  %-18s: %.*s\n", "Prefix", PREFIX_SIZE, this->prefix);
    SINUCA3_LOG_PRINTF("  %-18s: %d\n", "Version", this->version);
    SINUCA3_DEBUG_PRINTF(" %-18s: %ld\n", "Entries", this->entries);

    const char* targetStr = "Unknown";
    (void)targetStr;
    switch (this->target) {
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
            break;
    }

    SINUCA3_DEBUG_PRINTF(" %-18s: %s\n", "Target", targetStr);

    if (!printMetadata) return;

    if (this->type == FileTypeStaticTrace) {
        SINUCA3_DEBUG_PRINTF(" %-18s: %ld\n", "Instructions",
                             this->st.instructions);
        SINUCA3_DEBUG_PRINTF(" %-18s: %d\n", "Basic Blocks",
                             this->st.basicBlocks);
        SINUCA3_DEBUG_PRINTF(" %-18s: %d\n", "Threads", this->st.threads);
    } else if (this->type == FileTypeDynamicTrace) {
        SINUCA3_DEBUG_PRINTF(" %-18s: %ld\n", "Executed Instr.",
                             this->dyn.executed);
    }
}

void FileHeader::SetPrefix(FileType type) {
    switch (type) {
        case FileTypeStaticTrace:
            memcpy(this->prefix, PREFIX_STATIC_FILE, PREFIX_SIZE);
            break;
        case FileTypeDynamicTrace:
            memcpy(this->prefix, PREFIX_DYNAMIC_FILE, PREFIX_SIZE);
            break;
        case FileTypeMemoryTrace:
            memcpy(this->prefix, PREFIX_MEMORY_FILE, PREFIX_SIZE);
            break;
        default:
            assert(false && "Invalid file type");
    }
}

void FileHeader::Setup(TargetArchitecture target, EntriesCounter entries) {
    this->magic = TRACE_MAGIC;
    this->version = TRACE_VERSION;
    this->target = target;
    this->entries = entries;
}

void CompressedInstToStaticInfo(const CompressedInstruction* compressed,
                                StaticInstructionInfo* info) {
    assert(compressed != NULL);
    assert(info != NULL);

    strncpy(info->instMnemonic, compressed->instructionMnemonic,
            INST_MNEMONIC_LEN - 1);
    info->instMnemonic[INST_MNEMONIC_LEN - 1] = '\0';

    info->instSize = compressed->instructionSize;
    info->instAddress = compressed->instructionAddress;
    info->instPerformsAtomicUpdate = compressed->instPerformsAtomicUpdate;
    info->instCausesCacheLineFlush = compressed->instCausesCacheLineFlush;
    info->isPredicatedInst = compressed->isPredicatedInst;
    info->instReadsMemory = compressed->instReadsMemory;
    info->instWritesMemory = compressed->instWritesMemory;
    info->isIndirectControlFlowInst = compressed->isIndirectCtrlFlowInst;
    info->numberOfReadRegs = compressed->rRegsArrayOccupation;
    info->numberOfWriteRegs = compressed->wRegsArrayOccupation;

    long occupation = 0;

    occupation =
        sizeof(*compressed->readRegsArray) * compressed->rRegsArrayOccupation;
    memcpy(info->readRegsArray, compressed->readRegsArray, occupation);

    occupation = sizeof(*compressed->writtenRegsArray) *
                 compressed->wRegsArrayOccupation;
    memcpy(info->writtenRegsArray, compressed->writtenRegsArray, occupation);

    if (compressed->isCallInstruction) {
        info->branchType = BranchCall;
    } else if (compressed->isSyscallInstruction) {
        info->branchType = BranchSyscall;
    } else if (compressed->isRetInstruction) {
        info->branchType = BranchRet;
    } else if (compressed->isSysretInstruction) {
        info->branchType = BranchSysret;
    } else if (compressed->isBranchInstruction) {
        if (compressed->instHasFallthrough) {
            info->branchType = BranchCond;
        } else {
            info->branchType = BranchUncond;
        }
    }
}

bool IsValidEventType(int8_t eventType) {
    switch (eventType) {
        case EventTypeBarrierSync:
        case EventTypeCriticalStart:
        case EventTypeCriticalEnd:
        case EventTypeAbruptEnd:
            return true;
        default:
            return false;
    }
}

bool IsValidMemoryAccessType(int8_t accessType) {
    switch (accessType) {
        case MemoryAccessLoad:
        case MemoryAccessStore:
            return true;
        default:
            return false;
    }
}
