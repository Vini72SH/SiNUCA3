#ifndef SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_
#define SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_

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
 * @file file_handler.hpp
 * @brief Common trace file handling API.
 */

#include <cassert>
#include <cstdio>
#include <cstring>

#include <engine/default_packets.hpp>
#include <utils/logger.hpp>

extern "C" {
#include <stdint.h>
}

const uint32_t TRACE_VERSION = 2;
const uint32_t TRACE_MAGIC = 0x54524345;  // TRCE
const uint32_t PREFIX_SIZE = 6;
const uint32_t MAX_MEM_ACCESS_SIZE = 0x7F;

const char PREFIX_STATIC_FILE[PREFIX_SIZE] = "_S3S_";
const char PREFIX_DYNAMIC_FILE[PREFIX_SIZE] = "_S3D_";
const char PREFIX_MEMORY_FILE[PREFIX_SIZE] = "_S3M_";

typedef uint32_t MagicNumber;
typedef int64_t InstructionCounter;
typedef int32_t BasicBlockCounter;
typedef int32_t ThreadCounter;
typedef int32_t TraceVersion;
typedef int32_t BasicBlockSize;
typedef int32_t BasicBlockIdentifier;
typedef uint32_t MemoryAccessSize;
typedef int8_t MemoryAccessCounter;
typedef uint64_t MemoryAccessAddress;
typedef int64_t EntriesCounter;

enum FileType : int8_t {
    FileTypeStaticTrace,
    FileTypeDynamicTrace,
    FileTypeMemoryTrace,
    FileTypeUndef
};

enum TargetArchitecture : int8_t {
    TargetArchX86,
    TargetArchARM,
    TargetArchRISCV,
    TargetArchUndef
};

enum StaticTraceEntryType : int8_t {
    StaticEntryInstruction,
    StaticEntryBasicBlockSize
};

enum DynamicTraceEntryType : int8_t {
    DynamicEntryBasicBlockIdentifier,
    DynamicEntryEvent
};

enum MemoryEntryType : int8_t { MemoryEntryAccessCount, MemoryEntryAccess };

enum EventType : int8_t {
    EventTypeBarrierSync,
    EventTypeCriticalStart,
    EventTypeCriticalEnd,
    EventTypeAbruptEnd,
    EventTypeUndef
};

enum MemoryAccessType : int8_t { MemoryAccessLoad, MemoryAccessStore };

struct CompressedInstruction {
    uint64_t instructionAddress;
    uint64_t instructionSize;
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
} __attribute__((packed));

struct StaticTraceEntry {
    union {
        CompressedInstruction instruction;
        BasicBlockSize size;
    };

    StaticTraceEntryType type;

    inline void Set(const CompressedInstruction* inst) {
        this->type = StaticEntryInstruction;
        assert(inst != NULL);
        this->instruction = *inst;
    }
    inline void Set(const BasicBlockSize* size) {
        this->type = StaticEntryBasicBlockSize;
        assert(size != NULL);
        this->size = *size;
    }
    inline int Get(CompressedInstruction* inst) const {
        if (this->type != StaticEntryInstruction) return 1;
        assert(inst != NULL);
        *inst = this->instruction;
        return 0;
    }
    inline int Get(BasicBlockSize* size) const {
        if (this->type != StaticEntryBasicBlockSize) return 1;
        assert(size != NULL);
        *size = this->size;
        return 0;
    }
} __attribute__((packed));

struct DynamicTraceEntry {
    union {
        BasicBlockIdentifier bbl;
        EventType event;
    };

    DynamicTraceEntryType type;

    inline void Set(const BasicBlockIdentifier* idx) {
        this->type = DynamicEntryBasicBlockIdentifier;
        assert(idx != NULL);
        this->bbl = *idx;
    }
    inline void Set(const EventType* event) {
        this->type = DynamicEntryEvent;
        assert(event != NULL);
        this->event = *event;
    }
    inline int Get(BasicBlockIdentifier* idx) const {
        if (this->type != DynamicEntryBasicBlockIdentifier) return 1;
        assert(idx != NULL);
        *idx = this->bbl;
        return 0;
    }
    inline int Get(EventType* event) const {
        if (this->type != DynamicEntryEvent) return 1;
        assert(event != NULL);
        *event = this->event;
        return 0;
    }
} __attribute__((packed));

struct MemoryAccess {
    MemoryAccessAddress address;
    MemoryAccessType typeOfAccess;
    MemoryAccessSize size;
} __attribute__((packed));

struct MemoryTraceEntry {
    union {
        MemoryAccess access;
        MemoryAccessCounter count;
    };

    MemoryEntryType type;

    inline void Set(const MemoryAccess* access) {
        this->type = MemoryEntryAccess;
        assert(access != NULL);
        this->access = *access;
    }
    inline void Set(const MemoryAccessCounter* count) {
        this->type = MemoryEntryAccessCount;
        assert(count != NULL);
        this->count = *count;
    }
    inline int Get(MemoryAccess* access) const {
        if (this->type != MemoryEntryAccess) return 1;
        assert(access != NULL);
        *access = this->access;
        return 0;
    }
    inline int Get(MemoryAccessCounter* count) const {
        if (this->type != MemoryEntryAccessCount) return 1;
        assert(count != NULL);
        *count = this->count;
        return 0;
    }
} __attribute__((packed));

struct StaticFileMetadata {
    InstructionCounter instructions;
    BasicBlockCounter basicBlocks;
    ThreadCounter threads;
};

struct DynamicFileMetadata {
    InstructionCounter executed;
};

/** @brief File header for general usage. */
struct FileHeader {
    MagicNumber magic;
    char prefix[PREFIX_SIZE];

    FileType type;
    TargetArchitecture target;
    TraceVersion version;

    union {
        StaticFileMetadata st;
        DynamicFileMetadata dyn;
    };

    FileHeader() : magic(0), type(FileTypeUndef), target(TargetArchUndef), version(0) {
        this->prefix[0] = '\0';
    }

    FileHeader(FileType file, TargetArchitecture arch) : magic(TRACE_MAGIC), version(TRACE_VERSION) {
        this->type = file;
        this->target = arch;
        switch (this->type) {
        case FileTypeDynamicTrace:
            strncpy(this->prefix, PREFIX_DYNAMIC_FILE, sizeof(prefix));
            break;
        case FileTypeStaticTrace:
            strncpy(this->prefix, PREFIX_STATIC_FILE, sizeof(prefix));
            break;
        case FileTypeMemoryTrace:
            strncpy(this->prefix, PREFIX_MEMORY_FILE, sizeof(prefix));
            break;
        default:
            SINUCA3_WARNING_PRINTF("File type is invalid!\n");
            break;
        }
    }

    /** @brief Print the file header information. */
    void Print(bool printMetadata = false) const;

    inline void Set(const StaticFileMetadata* meta) {
        assert(meta != NULL);
        assert(magic != 0);
        this->st = *meta;
    }
    inline void Set(const DynamicFileMetadata* meta) {
        assert(meta != NULL);
        assert(magic!= 0);
        this->dyn = *meta;
    }
    inline int Get(StaticFileMetadata* meta) const {
        assert (this->type == FileTypeStaticTrace);
        assert(meta != NULL);
        *meta = this->st;
        return 0;
    }
    inline int Get(DynamicFileMetadata* meta) const {
        assert(this->type == FileTypeDynamicTrace);
        assert(meta != NULL);
        *meta = this->dyn;
        return 0;
    }
};

struct TraceFile {
    FILE* file;
    FileHeader header;

    inline TraceFile(const char* path, const char* mode) : file(0) {
        if (this->Open(path, mode) != 0)
            SINUCA3_LOG_PRINTF("Failed to open file: [%s] for [%s]\n", path,
                               mode);
    }
    inline int Write(const void* entry, long size) {
        if (this->file == NULL || entry == NULL) return 1;
        return (fwrite(entry, size, 1, this->file) != 1);
    }
    inline int Read(void* entry, long size) {
        if (this->file == NULL || entry == NULL) return 1;
        return (fread(entry, size, 1, this->file) != 1);
    }
    inline int WriteAt(const void* entry, long size, long offset) {
        if (this->file == NULL || entry == NULL) return 1;
        if (fseek(this->file, offset, SEEK_SET) != 0) return 1;
        return (this->Write(entry, size));
    }
    inline int Reserve(long bytes) {
        if (this->file == NULL) return 1;
        return (fseek(this->file, bytes, SEEK_CUR) != 0);
    }
    inline int Open(const char* path, const char* mode) {
        if (path == NULL) return 1;
        this->file = fopen(path, mode);
        return (this->file == NULL);
    }
    inline void Close() {
        if (this->file == NULL) return;
        fclose(this->file);
        this->file = NULL;
    }
    inline ~TraceFile() { this->Close(); }
};

template <typename T>
class Reader {
  private:
    TraceFile* trace;
    bool wasHeaderRead;

  public:
    inline Reader() : trace(0), wasHeaderRead(0) {}

    inline int Open(const char* path) {
        if (this->trace != NULL) return 1;
        this->trace = new TraceFile(path, "rb");
        if (this->trace->Read(&this->trace->header, sizeof(FileHeader)))
            return 1;
        this->wasHeaderRead = true;
        return 0;
    }
    template<typename U>
    inline int Read(U* data) {
        T entry;
        if (this->trace->Read(&entry, sizeof(T))) return 1;
        if (this->trace == NULL || data == NULL) return 1;
        return entry.Get(data);
    }
    inline int Read(T* entry) {
        if (this->trace == NULL || entry == NULL) return 1;
        return this->trace->Read(entry, sizeof(T));
    }
    inline int GetHeader(FileHeader* header) {
        if (this->trace == NULL || !this->wasHeaderRead) return 1;
        assert(header != NULL);
        *header = this->trace->header;
        return 0;
    }
    inline int EndOfFile() const {
        assert(this->trace != NULL);
        if (this->trace->file == NULL) return 1;
        return feof(this->trace->file);
    }
    inline ~Reader() {
        assert(this->wasHeaderRead);
        if (this->trace != NULL) {
            delete this->trace;
            this->trace = NULL;
        }
    }
};

template <typename T>
class Writer {
  private:
    TraceFile* trace;
    bool wasSpaceReserved;

  public:
    inline Writer() : trace(0), wasSpaceReserved(0) {}

    inline int Open(const char* path) {
        if (this->trace != NULL) return 1;
        this->trace = new TraceFile(path, "wb");
        if (this->trace->Reserve(sizeof(this->trace->header))) return 1;
        this->wasSpaceReserved = true;
        return 0;
    }
    template <typename U>
    inline int Write(const U* data) {
        T entry;
        if (this->trace == NULL || data == NULL) return 1;
        entry.Set(data);
        return this->trace->Write(&entry, sizeof(T));
    }
    inline int SetHeader(const FileHeader* header) {
        if (!this->trace || !this->wasSpaceReserved) return 1;
        assert(header != NULL);
        this->trace->header = *header;
        return 0;
    }
    inline ~Writer() {
        assert(this->wasSpaceReserved);
        if (this->trace != NULL) {
            this->trace->WriteAt(&this->trace->header, sizeof(FileHeader), 0);
            delete this->trace;
            this->trace = NULL;
        }
    }
};

/** @brief Convert a compressed instruction to a static instruction info. */
void CompressedInstToStaticInfo(const CompressedInstruction* compressedInst,
                                StaticInstructionInfo* staticInfo);

/** @brief Get alloc'd string with the formatted path for a trace file without
 * the thread id. */
const char* GetFormattedPath(const char* directory, const char* prefix);

/** @brief Get alloc'd string with the formatted path for a trace file with the
 * thread id. */
const char* GetFormattedPath(const char* directory, const char* prefix,
                             int tid);

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
void FormatPathTidIn(char* dest, const char* directory, const char* prefix,
                     int tid, long capacity);

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
void FormatPathTidOut(char* dest, const char* directory, const char* prefix,
                      long capacity);

/** @brief Check if the event type is valid. */
bool IsValidEventType(int8_t eventType);

/** @brief Check if the memory access type is valid. */
bool IsValidMemoryAccessType(int8_t accessType);

#endif
