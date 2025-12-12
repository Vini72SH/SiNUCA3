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
 * @file x86_file_handler.hpp
 * @brief Common file handling API for x86 traces.
 * @details It gathers constants, classes and functions used by the trace
 * generator based on intel pin for x86 architecture and the corresponding
 * trace reader. It is appropriate to have such file, as the traces for x86
 * architecture are binary files which implies a deep dependency on how the
 * the reading is done in relation to how the information is stored in the
 * traces. Therefore, maintaining the TraceFileWriter and TraceFileReader
 * implementations together allows for a better undestanding of how they
 * coexist.
 */

#include <cstdio>
#include <cstring>

#include "engine/default_packets.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
#include <errno.h>
#include <stdint.h>
}

#define _PACKED __attribute__((packed))

const int MAX_IMAGE_NAME_SIZE = 255;
const int RECORD_ARRAY_SIZE = 10000;
const int CURRENT_TRACE_VERSION = 1;
const short MAGIC_NUMBER = 187;

const char TRACE_TARGET_X86[] = "X86";
const char TRACE_TARGET_ARM[] = "ARM";
const char TRACE_TARGET_RISCV[] = "RISCV";
const char PREFIX_STATIC_FILE[] = "S3S";
const char PREFIX_DYNAMIC_FILE[] = "S3D";
const char PREFIX_MEMORY_FILE[] = "S3M";
const int PREFIX_SIZE = sizeof(PREFIX_STATIC_FILE);

enum FileType : uint8_t {
    FileTypeStaticTrace,
    FileTypeDynamicTrace,
    FileTypeMemoryTrace
};

enum TargetArch : uint8_t { TargetArchX86, TargetArchARM, TargetArchRISCV };

enum StaticTraceRecordType : uint8_t {
    StaticRecordInstruction,
    StaticRecordBasicBlockSize
};

enum DynamicTraceRecordType : uint8_t {
    DynamicRecordBasicBlockIdentifier,
    DynamicRecordCreateThread,
    DynamicRecordDestroyThread,
    DynamicRecordLockRequest,
    DynamicRecordUnlockRequest,
    DynamicRecordBarrier,
    DynamicRecordAbruptEnd
};

enum MemoryRecordType : uint8_t {
    MemoryRecordHeader,
    MemoryRecordLoad,
    MemoryRecordStore
};

/** @brief Instruction extracted informations */
struct Instruction {
    uint64_t instructionAddress;
    uint64_t instructionSize;
    uint32_t effectiveAddressWidth;
    uint16_t readRegsArray[MAX_REGISTERS];
    uint16_t writtenRegsArray[MAX_REGISTERS];
    uint8_t wRegsArrayOccupation;
    uint8_t rRegsArrayOccupation;
    uint8_t instHasFallthrough;
    uint8_t isBranchInstruction;
    uint8_t isSyscallInstruction;
    uint8_t isCallInstruction;
    uint8_t isRetInstruction;
    uint8_t isSysretInstruction;
    uint8_t isPrefetchHintInst;
    uint8_t isPredicatedInst;
    uint8_t isIndirectCtrlFlowInst;
    uint8_t instCausesCacheLineFlush;
    uint8_t instPerformsAtomicUpdate;
    uint8_t instReadsMemory;
    uint8_t instWritesMemory;
    char instructionMnemonic[INST_MNEMONIC_LEN];
} _PACKED;

/** @brief Written to static trace file. */
struct StaticTraceRecord {
    union _PACKED {
        uint16_t basicBlockSize;
        Instruction instruction;
    } data;
    uint8_t recordType;

    inline StaticTraceRecord() { memset(this, 0, sizeof(*this)); }
} _PACKED;

/** @brief Written to dynamic trace file. */
struct DynamicTraceRecord {
    union _PACKED {
        uint32_t basicBlockIdentifier;
        struct _PACKED {
            uint8_t isGlobalMutex;
            uint64_t mutexAddress;
        } lockInfo;
    } data;
    uint8_t recordType;

    inline DynamicTraceRecord() { memset(this, 0, sizeof(*this)); }
} _PACKED;

/** @brief Written to memory trace file. */
struct MemoryTraceRecord {
    union _PACKED {
        struct _PACKED {
            uint64_t address; /**<Virtual address accessed. */
            uint16_t size;    /**<Size in bytes of memory read or written. */
        } operation;
        int32_t numberOfMemoryOps;
    } data;
    uint8_t recordType;

    inline MemoryTraceRecord() { memset(this, 0, sizeof(*this)); }
} _PACKED;

/** @brief File header for general usage. */
struct FileHeader {
    union _PACKED {
        struct _PACKED {
            uint32_t instCount;
            uint32_t bblCount;
            uint16_t threadCount;
        } staticHeader;
        struct {
            uint64_t totalExecutedInstructions;
        } dynamicHeader;
    } data;
    uint8_t fileType;
    uint8_t traceVersion;
    uint8_t targetArch;

    inline FileHeader() {
        memset(this, 0, sizeof(*this));
    }
    int FlushHeader(FILE *file) {
        if (!file) return 1;
        rewind(file);

        unsigned long magicNumAndPrefSize = sizeof(MAGIC_NUMBER) + PREFIX_SIZE;
        char *magicNumAndPref = (char *)alloca(magicNumAndPrefSize);
        memcpy(magicNumAndPref, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
        magicNumAndPref[sizeof(MAGIC_NUMBER)] = '\0';

        if (this->fileType == FileTypeStaticTrace) {
            strcat(magicNumAndPref, PREFIX_STATIC_FILE);
        } else if (this->fileType == FileTypeDynamicTrace) {
            strcat(magicNumAndPref, PREFIX_DYNAMIC_FILE);
        } else {
            strcat(magicNumAndPref, PREFIX_MEMORY_FILE);
        }

        if (fwrite(magicNumAndPref, 1, magicNumAndPrefSize, file) !=
            magicNumAndPrefSize) {
            return 1;
        }
        if (fwrite(this, 1, sizeof(*this), file) != sizeof(*this)) {
            return 1;
        }

        return 0;
    }
    inline int LoadHeader(FILE *file) {
        if (!file) return 1;
        unsigned long magicNumAndPrefSize = sizeof(MAGIC_NUMBER) + PREFIX_SIZE;
        fseek(file, magicNumAndPrefSize, SEEK_SET);
        return (fread(this, 1, sizeof(*this), file) != sizeof(*this));
    }
    inline int LoadHeader(char **file) {
        if (!(*file)) return 1;
        unsigned long magicNumAndPrefSize = sizeof(MAGIC_NUMBER) + PREFIX_SIZE;
        *file = &(*file)[magicNumAndPrefSize];
        memcpy(this, *file, sizeof(*this));
        *file = &(*file)[sizeof(*this)];
        return 0;
    }
    inline void ReserveHeaderSpace(FILE *file) {
        int magicNumAndPrefSize = sizeof(MAGIC_NUMBER) + PREFIX_SIZE;
        fseek(file, magicNumAndPrefSize + sizeof(*this), SEEK_SET);
    }
} _PACKED;

inline void printFileErrorLog(const char *path, const char *mode) {
    SINUCA3_ERROR_PRINTF("Could not open [%s] in [%s] mode: ", path, mode);
    SINUCA3_ERROR_PRINTF("%s\n", strerror(errno));
}

/**
 * @brief Get max size of the formatted path string that includes the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param imageName Name of the executable used to generate the traces.
 */
unsigned long GetPathTidInSize(const char *sourceDir, const char *prefix,
                               const char *imageName);

/**
 * @brief Format the path in dest string including the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param imageName Name of the executable used to generate the traces.
 * @param tid Thread identier
 * @param destSize Max capacity of dest string.
 */
void FormatPathTidIn(char *dest, const char *sourceDir, const char *prefix,
                     const char *imageName, int tid, long destSize);

/**
 * @brief Get size of the formatted path string without the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'
 * @param imageName Name of the executable used to generate the traces.
 */
unsigned long GetPathTidOutSize(const char *sourceDir, const char *prefix,
                                const char *imageName);

/**
 * @brief Format the path in dest string without the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param imageName Name of the executable used to generate the traces.
 * @param destSize Max capacity of dest string.
 */
void FormatPathTidOut(char *dest, const char *sourceDir, const char *prefix,
                      const char *imageName, long destSize);

#endif
