#ifndef SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_
#define SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_

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

/**
 * @file file_handler.hpp
 * @brief Common trace file handling API.
 */

#include <cstdio>
#include <cstring>
#include <sinuca3.hpp>

extern "C" {
#include <alloca.h>
#include <stdint.h>
}

const uint32_t MAX_IMAGE_NAME_SIZE = 255;
const uint32_t TRACE_VERSION = 2;
const uint32_t TRACE_MAGIC = 0x54524345;
const uint32_t PREFIX_SIZE = 4;

const char PREFIX_STATIC_FILE[PREFIX_SIZE] = "S3S";
const char PREFIX_DYNAMIC_FILE[PREFIX_SIZE] = "S3D";
const char PREFIX_MEMORY_FILE[PREFIX_SIZE] = "S3M";

enum FileType : int8_t {
    FileTypeStaticTrace,
    FileTypeDynamicTrace,
    FileTypeMemoryTrace
};

enum TargetArchitecture : int8_t {
    TargetArchX86,
    TargetArchARM,
    TargetArchRISCV
};

enum StaticTraceEntryType : int8_t {
    StaticEntryInstruction,
    StaticEntryBasicBlockSize
};

enum DynamicTraceEntryType : int8_t {
    DynamicEntryBasicBlockIdentifier,
    DynamicEntryThreadEvent
};

enum ThreadEventType : uint32_t {
    ThreadEventBarrierSync,
    ThreadEventCriticalStart,
    ThreadEventCriticalEnd,
    ThreadEventAbruptEnd
};

enum MemoryEntryType : int8_t {
    MemoryEntryAccessCount,
    MemoryEntryLoad,
    MemoryEntryStore
};

/** @brief Instruction extracted informations */
struct CompressedInstruction {
    int64_t instructionAddress;
    int64_t instructionSize;
    int16_t readRegsArray[MAX_REGISTERS];
    int16_t writtenRegsArray[MAX_REGISTERS];
    int8_t wRegsArrayOccupation;
    int8_t rRegsArrayOccupation;
    int8_t instHasFallthrough;
    int8_t isBranchInstruction;
    int8_t isSyscallInstruction;
    int8_t isCallInstruction;
    int8_t isRetInstruction;
    int8_t isSysretInstruction;
    int8_t isPrefetchHintInst;
    int8_t isPredicatedInst;
    int8_t isIndirectCtrlFlowInst;
    int8_t instCausesCacheLineFlush;
    int8_t instPerformsAtomicUpdate;
    int8_t instReadsMemory;
    int8_t instWritesMemory;
    char instructionMnemonic[INST_MNEMONIC_LEN];
} __attribute__((packed));

/** @brief Written to static trace file. May contain either a basic block size
 * or an instruction. */
struct StaticTraceEntry {
    union {
        CompressedInstruction instruction;
        int32_t basicBlockSize;
    };
    StaticTraceEntryType entryType;
} __attribute__((packed));

/** @brief Written to dynamic trace file. May contain either a basic block
 * identifier or a thread event. */
struct DynamicTraceEntry {
    union {
        ThreadEventType threadEvent;
        int32_t basicBlock;
    };
    DynamicTraceEntryType entryType;
} __attribute__((packed));

/** @brief Written to memory trace file. May contain either a memory access or a
 * memory header. */
struct MemoryTraceEntry {
    union {
        struct {
            int64_t virtualAddress;
            int32_t accessSize;
        };
        int32_t memoryAccessCount;
    };
    MemoryEntryType entryType;
} __attribute__((packed));

/** @brief File header for general usage. */
struct FileHeader {
    uint32_t magicNumber;
    int8_t prefix[PREFIX_SIZE];

    FileType fileType;
    TargetArchitecture targetArch;
    uint32_t traceVersion;

    union {
        struct {
            int32_t recordedInstructions;
            int32_t recordedBasicBlocks;
            int32_t registeredThreads;
        } staticHeader;
        struct {
            int64_t executedInstructions;
        } dynamicHeader;
    };

    inline FileHeader() {
        memset(this, 0, sizeof(*this));
        magicNumber = TRACE_MAGIC;
        traceVersion = TRACE_VERSION;
    }

    /** @brief Adjust file pointer. The header is generally written at file
     * clousure, so the file ptr must be moved to leave enough space for it. */
    int ReserveHeaderSpace(FILE* file);
    /** @brief Set the static header values. */
    void SetStaticHeader(int32_t inst, int32_t bbls, int32_t threads);
    /** @brief Set the dynamic header values. */
    void SetDynamicHeader(int64_t executedInstructions);
    /** @brief Get the static header values. */
    int GetRecordedInstructions();
    /** @brief Get the recorded basic blocks. */
    int GetRecordedBasicBlocks();
    /** @brief Get the registered threads. */
    int GetRegisteredThreads();
    /** @brief Get the dynamic header values. */
    long GetExecutedInstructions();
    /** @brief Print the version and target architecture. */
    inline void PrintVersionAndTarget() const;
    /** @brief Check if the header is valid. */
    inline bool IsValid() const {
        return (magicNumber == TRACE_MAGIC) && (traceVersion == TRACE_VERSION);
    }
};

/** @brief Convert a compressed instruction to a static instruction info. */
int CompressedInstToStaticInfo(const CompressedInstruction* compressedInst, StaticInstructionInfo* staticInfo);

/** @brief Write an entry to the file. */
template<typename T>
int WriteEntry(FILE* file, const T* entry) {
    if (file == NULL || entry == NULL) {
        return 1;
    }
    return (fwrite(entry, sizeof(*entry), 1, file) != 1);
}

/** @brief Read an entry from the file. */
template<typename T>
int ReadEntry(FILE* file, T* entry) {
    if (file == NULL || entry == NULL) {
        return 1;
    }
    return (fread(entry, sizeof(*entry), 1, file) != 1);
}

/**
 * @brief Get max size of the formatted path string that includes the thread id.
 * @param directory Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param executable Name of the executable used to generate the traces.
 */
long GetPathTidInSize(const char* directory, const char* prefix);

/**
 * @brief Format the path in dest string including the thread id.
 * @param directory Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param executable Name of the executable used to generate the traces.
 * @param tid Thread identifier
 * @param capacity Max capacity of dest string.
 */
void FormatPathTidIn(char* dest, const char* directory, const char* prefix, int tid, long capacity);

/**
 * @brief Get size of the formatted path string without the thread id.
 * @param directory Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'
 * @param executable Name of the executable used to generate the traces.
 */
long GetPathTidOutSize(const char* directory, const char* prefix);

/**
 * @brief Format the path in dest string without the thread id.
 * @param directory Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param executable Name of the executable used to generate the traces.
 * @param capacity Max capacity of dest string.
 */
void FormatPathTidOut(char* dest, const char* directory, const char* prefix, long capacity);

#endif
