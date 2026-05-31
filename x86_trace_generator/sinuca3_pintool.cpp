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
 * @file sinuca3_pintool.cpp
 * @brief Implementation of the SiNUCA3 x86_64 tracer based on Intel Pin.
 *
 * @note Instrumentation is the process of deciding where and what code
 * should be inserted into the target program, while analysis refers to the
 * code that is actually executed at those insertion points to gather
 * information about the program’s behavior.
 *
 * @details To enable instrumentation, wrap the target code with
 * BeginInstrumentationBlock() and EndInstrumentationBlock(). Instrumentation
 * code is only inserted within these blocks.
 */

#include <cassert>

#define NDEBUG

#include <tracer/sinuca/file_handler.hpp>

#include "intrinsics.hpp"
#include "pin.H"

extern "C" {
#include <sys/stat.h>
#include <unistd.h>
}

/**
 * @brief When enabled, this flag allows the instrumentation phase to insert
 * analysis code into the target program.
 */
bool isInstrumentating;
/** @brief Flag indicating whether instrumentation has been initialized. */
bool wasInitInstrumentationCalled = false;

char* pathToStaticFile = NULL;

/** @brief Directory to store the trace files. */
std::string directory;
/** @brief OpenMP routines to ignore. */
std::vector<const char*> routinesToIgnore;
/** @brief List of intrinsic instructions. */
std::vector<IntrinsicInfo> intrinsics;

/** @brief Logger for instruction traces. */
Writer instructionsLog;
/** @brief Metadata for the static trace file. */
StaticFileMetadata instLogMetadata;

EntriesCounter entriesStaticTrace = 0;
EntriesCounter entriesDynamicTrace = 0;
EntriesCounter entriesMemoryTrace = 0;

PIN_LOCK analysisLock;

/* A KNOB is a class that encapsulates a command line argument. When the
 * argument is not provided, the default value declared is used. */
KNOB<std::string> knobFolder(KNOB_MODE_WRITEONCE, "pintool", "o", "./",
                             "Directory to store the trace files.");
KNOB<BOOL> knobForceInstrumentation(KNOB_MODE_WRITEONCE, "pintool", "f", "0",
                                    "Force instrumentation.");
KNOB<UINT32> knobNumberOfInstructions(KNOB_MODE_WRITEONCE, "pintool", "n", "-1",
                                      "Set maximum of instructions.");

struct Thread {
    Writer executionLog;
    Writer memoryLog;

    char* pathToExecLog;
    char* pathToMemLog;

    DynamicFileMetadata execLogMetadata;

    bool isInstrumentating;

    Thread() : pathToExecLog(0), pathToMemLog(0) {
        this->isInstrumentating = true;
        this->execLogMetadata.executed = 0;
    }
    ~Thread() {
        FileHeader headerDynamic;
        headerDynamic.Setup(TargetArchX86, entriesDynamicTrace);
        headerDynamic.Set(&this->execLogMetadata);
        this->executionLog.SetHeader(&headerDynamic);

        FileHeader headerMemory;
        headerMemory.Setup(TargetArchX86, entriesMemoryTrace);
        headerMemory.SetFileType(FileTypeMemoryTrace);
        this->memoryLog.SetHeader(&headerMemory);

        if (this->pathToExecLog) {
            delete[] this->pathToExecLog;
        }
        if (this->pathToMemLog) {
            delete[] this->pathToMemLog;
        }

#ifndef NDEBUG
        SINUCA3_LOG_PRINTF("Header from dynamic file: \n");
        headerDynamic.Print(true);
        SINUCA3_LOG_PRINTF("Header from memory file: \n");
        headerMemory.Print(true);
#endif
    }
};

std::vector<Thread*> threads;

int OpenTraceAndCreateLogger(const char* dir, const char* prefix, char** path,
                             int tid, bool tidOut, Writer* logger) {
    assert(dir != NULL);
    assert(prefix != NULL);
    assert(path != NULL);
    assert(logger != NULL);

    char* tracePath = NULL;

    if (tidOut) {
        long pathSize = GetPathTidOutSize(dir, prefix);
        tracePath = new char[pathSize];
        FormatPathTidOut(tracePath, dir, prefix, pathSize);
    } else {
        long pathSize = GetPathTidInSize(dir, prefix);
        tracePath = new char[pathSize];
        FormatPathTidIn(tracePath, dir, prefix, tid, pathSize);
    }

    if (logger->Open(tracePath)) {
        SINUCA3_ERROR_PRINTF("Failed to open trace file [%s]!\n", tracePath);
        delete[] tracePath;
        return 1;
    }

    *path = tracePath;

    return 0;
}

MemoryTraceEntry CreateEntryForMemoryTrace(MemoryAccessAddress add,
                                           MemoryAccessSize size,
                                           MemoryAccessType type) {
    MemoryAccess access;
    access.address = add;
    access.typeOfAccess = type;
    access.size = size;
    MemoryTraceEntry entry;
    entry.Set(&access);
    return entry;
}

MemoryTraceEntry CreateEntryForMemoryTrace(MemoryAccessCounter accesses) {
    MemoryTraceEntry entry;
    entry.Set(&accesses);
    return entry;
}

DynamicTraceEntry CreateEntryForDynamicTrace(EventType event) {
    DynamicTraceEntry entry;
    entry.Set(&event);
    return entry;
}

DynamicTraceEntry CreateEntryForDynamicTrace(BasicBlockIdentifier bbl) {
    DynamicTraceEntry entry;
    entry.Set(&bbl);
    return entry;
}

StaticTraceEntry CreateEntryForStaticTrace(CompressedInstruction* inst) {
    StaticTraceEntry entry;
    entry.Set(inst);
    return entry;
}

StaticTraceEntry CreateEntryForStaticTrace(BasicBlockSize size) {
    StaticTraceEntry entry;
    entry.Set(&size);
    return entry;
}

inline BasicBlockCounter GetBasicBlockCount() {
    return instLogMetadata.basicBlocks;
}
inline void IncrementBasicBlockCount() {
    instLogMetadata.basicBlocks++;
}
inline InstructionCounter GetStaticInstructionCount() {
    return instLogMetadata.instructions;
}
inline void IncrementStaticInstructionCount() {
    instLogMetadata.instructions++;
}
inline ThreadCounter GetThreadCount() {
    return instLogMetadata.threads;
}
inline void IncrementThreadCount() {
    instLogMetadata.threads++;
}
inline InstructionCounter GetExecutedInstructionCount(int tid) {
    return threads[tid]->execLogMetadata.executed;
}
inline void UpdateExecutedInstructionCount(int tid, InstructionCounter count) {
    threads[tid]->execLogMetadata.executed += count;
}

int Usage() {
    SINUCA3_LOG_PRINTF(
        "SiNUCA3 Pin Tool Usage\n"
        "------------------------------------------------------------\n"
        "Example:\n"
        "\t./pin/pin -t ./obj-intel64/my_pintool.so -o my_dir -- ./my_program\n\n"
        "Options:\n"
        "  -o <dir>\tOutput directory (default: ./)\n"
        "  -f\t\tForce instrumentation even when no blocks are defined\n"
        "  -n <num>\tMaximum number of instructions to append to trace\n"
        "  -i <list>\tSet intrinsics\n"
        "------------------------------------------------------------\n");

    return 1;
}

bool WasThreadCreated(THREADID tid) { return ((threads.size() - tid) > 0); }

/** @brief Enables instrumentation. */
VOID InitInstrumentation() {
    if (isInstrumentating) return;
    SINUCA3_DEBUG_PRINTF("========================================\n");
    SINUCA3_DEBUG_PRINTF(">> Beginning tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("   Output directory        : %s\n", directory.c_str());
    SINUCA3_DEBUG_PRINTF("   Force instrumentation   : %s\n",
                         knobForceInstrumentation.Value() ? "yes" : "no");
    SINUCA3_DEBUG_PRINTF("   Max instructions        : %u\n",
                         knobNumberOfInstructions.Value());
    SINUCA3_DEBUG_PRINTF("========================================\n");
    wasInitInstrumentationCalled = true;
    isInstrumentating = true;
}

/** @brief Resume instrumentation in a thread. */
VOID ResumeInstrumentationInThread(THREADID tid) {
    assert(WasThreadCreated(tid));
    threads[tid]->isInstrumentating = true;
}

/** @brief Disable instrumentation. */
VOID StopInstrumentation() {
    if (!isInstrumentating || knobForceInstrumentation.Value()) return;
    SINUCA3_DEBUG_PRINTF("========================================\n");
    SINUCA3_DEBUG_PRINTF(">> End of tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("========================================\n");
    isInstrumentating = false;
}

/** @brief Disable instrumentation in a thread. */
VOID StopInstrumentationInThread(THREADID tid) {
    assert(WasThreadCreated(tid));
    threads[tid]->isInstrumentating = false;
}

/** @brief Set up thread data */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    Thread* thread = new Thread;

    if (OpenTraceAndCreateLogger(directory.c_str(), "dynamic", &thread->pathToExecLog,
                                 tid, false, &thread->executionLog)) {
        SINUCA3_ERROR_PRINTF("Failed to open dynamic trace!\n");
    }
    if (OpenTraceAndCreateLogger(directory.c_str(), "memory", &thread->pathToMemLog,
                                 tid, false, &thread->memoryLog)) {
        SINUCA3_ERROR_PRINTF("Failed to open memory trace!\n");
    }

    PIN_GetLock(&analysisLock, tid);
    threads.push_back(thread);
    IncrementThreadCount();
    PIN_ReleaseLock(&analysisLock);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    assert(WasThreadCreated(tid));
    PIN_GetLock(&analysisLock, tid);
    SINUCA3_DEBUG_PRINTF("Thread [%d] end of execution!\n", tid);
    delete threads[tid];
    PIN_ReleaseLock(&analysisLock);
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(THREADID tid, UINT32 bbl, UINT64 inst) {
    assert(WasThreadCreated(tid));

    UpdateExecutedInstructionCount(tid, inst);

    PIN_GetLock(&analysisLock, tid);

    if (knobNumberOfInstructions.Value() != UINT_MAX) {
        if (threads[tid]->execLogMetadata.executed > knobNumberOfInstructions.Value()) {
            SINUCA3_WARNING_PRINTF("Reached maximum of instructions!\n");
            /* This loop adds an abrupt end event to the dynamic trace, which
             * signals to the trace reader that all analysis code was abruptly
             * halted. */
            for (unsigned tid = 0; tid < threads.size(); ++tid) {
                EventType event = EventTypeAbruptEnd;
                DynamicTraceEntry entry = CreateEntryForDynamicTrace(event);
                threads[tid]->executionLog.Write(&entry);
                ++entriesDynamicTrace;
            }
            PIN_ReleaseLock(&analysisLock);
            PIN_ExitApplication(0);
        }
    }

    BasicBlockIdentifier idx = bbl;
    DynamicTraceEntry entry = CreateEntryForDynamicTrace(idx);
    threads[tid]->executionLog.Write(&entry);
    ++entriesDynamicTrace;

    PIN_ReleaseLock(&analysisLock);
}

/** @brief Add memory operations to trace. */
VOID AppendToMemTrace(THREADID tid, PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    assert(WasThreadCreated(tid));

    /* Save the number of memory operations to fetch from the trace. */
    int count = accessInfo->numberOfMemops;
    int countCopy = count;

    for (int i = 0; i < countCopy; ++i)
        if (!accessInfo->memop[i].maskOn) --count;

    MemoryAccessCounter accesses = count;
    MemoryTraceEntry entry = CreateEntryForMemoryTrace(accesses);
    threads[tid]->memoryLog.Write(&entry);
    ++entriesMemoryTrace;

    /* Write memory operations to file */
    for (int i = 0; i < countCopy; i++) {
        if (!accessInfo->memop[i].maskOn) continue;

        MemoryAccessAddress addr = accessInfo->memop[i].memoryAddress;
        MemoryAccessSize size = accessInfo->memop[i].bytesAccessed;
        MemoryAccessType type;

        if (accessInfo->memop[i].memopType == PIN_MEMOP_LOAD)
            type = MemoryAccessLoad;
        else
            type = MemoryAccessStore;

        MemoryTraceEntry entry = CreateEntryForMemoryTrace(addr, size, type);
        threads[tid]->memoryLog.Write(&entry);
        ++entriesMemoryTrace;
    }
}

int TranslatePinInst(CompressedInstruction* inst, const INS* pinInst) {
    assert(inst != NULL);
    assert(pinInst != NULL);

    memset(inst, 0, sizeof(*inst));

    std::string mnemonic = INS_Mnemonic(*pinInst);
    long size = sizeof(inst->instructionMnemonic) - 1;

    assert(size >= (long)mnemonic.size());

    strncpy(inst->instructionMnemonic, mnemonic.c_str(), size);

    inst->instructionAddress = INS_Address(*pinInst);
    /* at most 15 bytes len (for now) */
    inst->instructionSize = INS_Size(*pinInst);
    /* manual flush with CLFLUSH/CLFLUSHOPT/CLWB/WBINVD/INVD */
    /* or cache coherence induced flush */
    inst->instCausesCacheLineFlush = INS_IsCacheLineFlush(*pinInst);
    /* false for any instruction which in practice is a system call */
    inst->isCallInstruction = INS_IsCall(*pinInst);
    inst->isSyscallInstruction = INS_IsSyscall(*pinInst);
    inst->isRetInstruction = INS_IsRet(*pinInst);
    inst->isSysretInstruction = INS_IsSysret(*pinInst);
    /* false for unconditional branches and calls */
    inst->instHasFallthrough = INS_HasFallThrough(*pinInst);
    /* false for system call */
    inst->isBranchInstruction = INS_IsBranch(*pinInst);
    inst->isIndirectCtrlFlowInst = INS_IsIndirectControlFlow(*pinInst);
    /* field checked before reading from memory trace */
    inst->instReadsMemory = INS_IsMemoryRead(*pinInst);
    inst->instWritesMemory = INS_IsMemoryWrite(*pinInst);
    /* e.g. CMOV */
    inst->isPredicatedInst = INS_IsPredicated(*pinInst);
    inst->instPerformsAtomicUpdate = INS_IsAtomicUpdate(*pinInst);

    for (long i = 0; i < INS_OperandCount(*pinInst); ++i) {
        /* Interest only in register operands */
        if (!INS_OperandIsReg(*pinInst, i)) {
            continue;
        }

        const unsigned long readRegsArraySize =
            sizeof(inst->readRegsArray) / sizeof(*inst->readRegsArray);
        const unsigned long writeRegsArraySize =
            sizeof(inst->writtenRegsArray) / sizeof(*inst->writtenRegsArray);

        short reg = INS_OperandReg(*pinInst, i);
        if (INS_OperandRead(*pinInst, i)) {
            if (inst->rRegsArrayOccupation >= readRegsArraySize) {
                SINUCA3_ERROR_PRINTF(
                    "More registers read than readRegsArray can store\n");
                return 1;
            }
            inst->readRegsArray[inst->rRegsArrayOccupation] = reg;
            ++inst->rRegsArrayOccupation;
        }
        if (INS_OperandWritten(*pinInst, i)) {
            if (inst->wRegsArrayOccupation >= writeRegsArraySize) {
                SINUCA3_ERROR_PRINTF(
                    "More registers written than writtenRegsArray can store\n");
                return 1;
            }
            inst->writtenRegsArray[inst->wRegsArrayOccupation] = reg;
            ++inst->wRegsArrayOccupation;
        }
    }

    return 0;
}

VOID OnTrace(TRACE trace, VOID* ptr) {
    int threadId = PIN_ThreadId();
    assert(WasThreadCreated(threadId));

    if (!isInstrumentating || !threads[threadId]->isInstrumentating) return;

    RTN rtn = TRACE_Rtn(trace);
    if (!RTN_Valid(rtn)) {
        SINUCA3_ERROR_PRINTF("Found invalid routine! Skipping...\n");
        return;
    }

    RTN_Open(rtn);
    std::string rtnName = RTN_Name(rtn);
    RTN_Close(rtn);

    /*
     * Remove unwanted spinlock.
     */
    for (unsigned int it = 0; it < routinesToIgnore.size(); it++) {
        if (rtnName == routinesToIgnore[it]) {
            SINUCA3_DEBUG_PRINTF("Thread id [%d]: Ignoring [%s]!\n", threadId,
                                 rtnName.c_str());
            return;
        }
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BasicBlockSize size = BBL_NumIns(bbl);
        StaticTraceEntry entry = CreateEntryForStaticTrace(size);
        instructionsLog.Write(&entry);
        ++entriesStaticTrace;

        BasicBlockIdentifier idx = GetBasicBlockCount();
        IncrementBasicBlockCount();

        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_THREAD_ID, IARG_UINT32, idx, IARG_UINT64, size,
                       IARG_END);

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            IncrementStaticInstructionCount();

            CompressedInstruction compressed;
            IntrinsicInfo* intrinsic = GetIntrinsicInfo(&ins);
            bool isIntrinsic = (intrinsic != NULL);

            if (isIntrinsic) {
                IntrinsicToSinucaInst(&ins, intrinsic, &compressed);
                StaticTraceEntry entry = CreateEntryForStaticTrace(&compressed);
                instructionsLog.Write(&entry);
                ++entriesStaticTrace;
                continue;
            }

            TranslatePinInst(&compressed, &ins);
            StaticTraceEntry entry = CreateEntryForStaticTrace(&compressed);
            instructionsLog.Write(&entry);
            ++entriesStaticTrace;

            if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTrace,
                               IARG_THREAD_ID, IARG_MULTI_MEMORYACCESS_EA,
                               IARG_END);
            }
        }
    }
}

VOID OnExecutionEvent(THREADID tid, UINT32 event, BOOL onlyInMaster) {
    assert(WasThreadCreated(tid));

    PIN_GetLock(&analysisLock, tid);

    EventType type = (EventType)event;
    DynamicTraceEntry entry = CreateEntryForDynamicTrace(type);
    threads[tid]->executionLog.Write(&entry);
    ++entriesDynamicTrace;

    /* If the event only happens in master thread, it must be manually added to child threads. */
    if (onlyInMaster) {
        SINUCA3_DEBUG_PRINTF("Add event [%u] to all [%u] threads!\n", event, threads.size());
        for (unsigned i = 1; i < threads.size(); i++) {
            EventType type = (EventType)event;
            DynamicTraceEntry entry = CreateEntryForDynamicTrace(type);
            threads[i]->executionLog.Write(&entry);
            ++entriesDynamicTrace;
        }
    }

    PIN_ReleaseLock(&analysisLock);
}

VOID OnImageLoad(IMG img, VOID* ptr) {
    if (!IMG_IsMainExecutable(img)) return;

    std::string absoluteImgPath = IMG_Name(img);
    long idx = absoluteImgPath.find_last_of('/') + 1;
    std::string application = absoluteImgPath.substr(idx);

    directory = directory + "/" + application + "/";
    if (access(directory.c_str(), F_OK) != 0) {
        mkdir(directory.c_str(), S_IRWXU | S_IRWXG | S_IROTH);
    }

    if (OpenTraceAndCreateLogger(directory.c_str(), "static", &pathToStaticFile, 0,
                                 true, &instructionsLog)) {
        SINUCA3_ERROR_PRINTF("Failed to open static trace!\n");
    }

    /* Only thread master run these calls. */
    std::vector<const char*> ompBarrierMasterStartVec;
    ompBarrierMasterStartVec.push_back("gomp_team_start");
    /* All threads run these calls. */
    std::vector<const char*> ompBarrierSimpleVec;
    ompBarrierSimpleVec.push_back(("GOMP_barrier"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_dynamic_start"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_ordered_static_start"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_guided_start"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_end"));
    ompBarrierSimpleVec.push_back(("GOMP_parallel_sections_start"));
    ompBarrierSimpleVec.push_back(("GOMP_sections_end"));
    /* Critical region begin. */
    std::vector<const char*> ompCriticalStartVec;
    ompCriticalStartVec.push_back(("GOMP_atomic_start"));
    ompCriticalStartVec.push_back(("GOMP_critical_start"));
    ompCriticalStartVec.push_back(("GOMP_critical_name_start"));
    /* Critical region end. */
    std::vector<const char*> ompCriticalEndVec;
    ompCriticalEndVec.push_back(("GOMP_atomic_end"));
    ompCriticalEndVec.push_back(("GOMP_critical_end"));
    ompCriticalEndVec.push_back(("GOMP_critical_name_end"));
    /* Routines to not instrument. */
    routinesToIgnore.push_back("gomp_mutex_lock_slow");
    routinesToIgnore.push_back("gomp_sem_wait_slow");
    routinesToIgnore.push_back("gomp_ptrlock_get_slow");
    routinesToIgnore.push_back("gomp_barrier_wait_end");
    routinesToIgnore.push_back("pthread_mutex_lock");
    routinesToIgnore.push_back("pthread_mutex_cond_lock");
    routinesToIgnore.push_back("pthread_spinlock");
    routinesToIgnore.push_back("pthread_mutex_timedlock");

    /* Instrumentation control. */
    const char* INST_START = "BeginInstrumentationBlock";
    const char* INST_END = "EndInstrumentationBlock";

    bool routineTreated;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);

            std::string rtnName = RTN_Name(rtn);
            routineTreated = false;
            unsigned it;

            if (rtnName == INST_START) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InitInstrumentation,
                               IARG_END);
                routineTreated = true;
            } else if (rtnName == INST_END) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopInstrumentation,
                               IARG_END);
                routineTreated = true;
            }

            if (rtnName.compare(0, 4, "gomp") &&
                rtnName.compare(0, 4, "GOMP")) {
                RTN_Close(rtn);
                continue;
            }

            for (it = 0;
                 it < ompBarrierMasterStartVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompBarrierMasterStartVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)OnExecutionEvent,
                                   IARG_THREAD_ID, IARG_UINT32,
                                   EventTypeBarrierSync, IARG_BOOL, true,
                                   IARG_END);
                    routineTreated = true;
                }
            }
            for (it = 0; it < ompBarrierSimpleVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompBarrierSimpleVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)OnExecutionEvent, IARG_THREAD_ID,
                                   IARG_UINT32, EventTypeBarrierSync,
                                   IARG_BOOL, false, IARG_END);
                    routineTreated = true;
                }
            }
            for (it = 0; it < ompCriticalStartVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompCriticalStartVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)OnExecutionEvent, IARG_THREAD_ID,
                                   IARG_UINT32, EventTypeCriticalStart,
                                   IARG_BOOL, false, IARG_END);
                    routineTreated = true;
                }
            }
            for (it = 0; it < ompCriticalEndVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompCriticalEndVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)OnExecutionEvent, IARG_THREAD_ID,
                                   IARG_UINT32, EventTypeCriticalEnd,
                                   IARG_BOOL, false, IARG_END);
                    routineTreated = true;
                }
            }

            for (IntrinsicInfo& intrinsic : intrinsics) {
                if (rtnName == intrinsic.loaderName) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)StopInstrumentationInThread,
                                   IARG_THREAD_ID, IARG_END);
                    RTN_InsertCall(rtn, IPOINT_AFTER,
                                   (AFUNPTR)ResumeInstrumentationInThread,
                                   IARG_THREAD_ID, IARG_END);
                    break;
                }
            }

            RTN_Close(rtn);
        }
    }
}

VOID OnFini(INT32 code, VOID* ptr) {
    FileHeader header;
    header.Setup(TargetArchX86, entriesStaticTrace);
    header.Set(&instLogMetadata);
    instructionsLog.SetHeader(&header);

#ifndef NDEBUG
    SINUCA3_LOG_PRINTF("Header from static trace: \n");
    header.Print(true);
#endif

    if (pathToStaticFile != NULL) {
        delete[] pathToStaticFile;
    }

    if (!wasInitInstrumentationCalled) {
        SINUCA3_WARNING_PRINTF(
            "No instrumentation blocks were found in the target program!\n\n");
    }
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    directory = knobFolder.Value();
    if (directory.back() == '/') {
        directory.pop_back();
    }

    PIN_InitLock(&analysisLock);

    if (knobForceInstrumentation.Value()) {
        SINUCA3_WARNING_PRINTF("Instrumenting entire program\n");
        InitInstrumentation();
    } else {
        isInstrumentating = false;
    }

    LoadIntrinsics();

    IMG_AddInstrumentFunction(OnImageLoad, NULL);
    TRACE_AddInstrumentFunction(OnTrace, NULL);
    PIN_AddFiniFunction(OnFini, NULL);

    PIN_AddThreadStartFunction(OnThreadStart, NULL);
    PIN_AddThreadFiniFunction(OnThreadFini, NULL);

    PIN_StartProgram();

    return 0;
}
