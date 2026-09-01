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
#include <filesystem>
#include <queue>
#include <string>
#include <tracer/sinuca/file_handler.hpp>
#include <utils/logger.hpp>
#include <vector>

#include "intrinsics.hpp"
#include "pin.H"

struct ThreadData {
    bool isInstrumenting{true};

    Writer<DynamicTraceEntry> dynamicTraceLogger;
    Writer<MemoryTraceEntry> memoryTraceLogger;

    std::string pathToDynFile{};
    std::string pathToMemFile{};
    std::queue<EventType> requests{};

    DynamicFileMetadata dynamicTraceMetadata{.executed{0}};
};

struct GlobalData {
    bool isInstrumenting{false};
    bool hasInitiatedInstrumenting{false};

    std::string pathToStFile{};
    std::string traceDirectory{};
    std::vector<std::string> ignore{};

    InstructionCounter totalExecuted{0};
    Writer<StaticTraceEntry> staticTraceLogger;
    StaticFileMetadata staticTraceMetadata{
        .instructions{0}, .basicBlocks{0}, .threads{0}};
} global;

std::vector<IntrinsicInfo> intrinsics{};

PIN_LOCK globalDataLock;
PIN_LOCK threadRequestLock;

/** @brief key for accessing TLS storage in the threads. */
TLS_KEY tlsKey = INVALID_TLS_KEY;

/* A KNOB is a class that encapsulates a command line argument. When the
 * argument is not provided, the default value declared is used. */
KNOB<std::string> knobFolder(KNOB_MODE_WRITEONCE, "pintool", "o", "./",
                             "Directory to store the trace files.");
KNOB<BOOL> knobForceInstrumentation(KNOB_MODE_WRITEONCE, "pintool", "f", "0",
                                    "Force instrumentation.");
KNOB<UINT32> knobNumberOfInstructions(KNOB_MODE_WRITEONCE, "pintool", "n", "-1",
                                      "Set maximum of instructions.");

int Usage() {
    SINUCA3_LOG_PRINTF(
        "SiNUCA3 Pin Tool Usage\n"
        "------------------------------------------------------------\n"
        "Example:\n"
        "\t./pin/pin -t ./obj-intel64/my_pintool.so -o my_dir -- "
        "./my_program\n\n"
        "Options:\n"
        "  -o <dir>\tOutput directory (default: ./)\n"
        "  -f\t\tForce instrumentation even when no blocks are defined\n"
        "  -n <num>\tMaximum number of instructions to append to trace\n"
        "  -i <list>\tSet intrinsics\n"
        "------------------------------------------------------------\n");

    return 1;
}

/** @brief Enables instrumentation. */
VOID InitInstrumentation() {
    THREADID id = PIN_ThreadId();
    PIN_GetLock(&globalDataLock, id + 1);

    if (global.isInstrumenting) {
        SINUCA3_WARNING_PRINTF("Instrumentation already active!\n");
        PIN_ReleaseLock(&globalDataLock);
        return;
    }

    SINUCA3_DEBUG_PRINTF("========================================\n");
    SINUCA3_DEBUG_PRINTF(">> Beginning tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("   Output directory        : %s\n",
                         global.traceDirectory.c_str());
    SINUCA3_DEBUG_PRINTF("   Force instrumentation   : %s\n",
                         knobForceInstrumentation.Value() ? "yes" : "no");
    SINUCA3_DEBUG_PRINTF("   Max instructions        : %u\n",
                         knobNumberOfInstructions.Value());
    SINUCA3_DEBUG_PRINTF("========================================\n");

    global.hasInitiatedInstrumenting = true;
    global.isInstrumenting = true;

    PIN_ReleaseLock(&globalDataLock);
}

VOID ResumeInstrumentationInThread(THREADID tid) {
    ThreadData* data = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    assert(data != nullptr && "Invalid thread data!\n");
    data->isInstrumenting = true;
}

/** @brief Disable instrumentation. */
VOID StopInstrumentation() {
    THREADID id = PIN_ThreadId();
    PIN_GetLock(&globalDataLock, id + 1);

    if (!global.isInstrumenting || knobForceInstrumentation.Value()) {
        PIN_ReleaseLock(&globalDataLock);
        return;
    }

    SINUCA3_DEBUG_PRINTF("========================================\n");
    SINUCA3_DEBUG_PRINTF(">> End of tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("========================================\n");

    global.isInstrumenting = false;

    PIN_ReleaseLock(&globalDataLock);
}

VOID StopInstrumentationInThread(THREADID tid) {
    ThreadData* data = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    assert(data != nullptr && "Invalid thread data!\n");
    data->isInstrumenting = false;
}

VOID CheckForThreadRequests(THREADID tid, ThreadData* data) {
    PIN_GetLock(&threadRequestLock, tid + 1);
    assert(data != nullptr && "Invalid thread data!\n");
    while (!data->requests.empty()) {
        EventType req{EventTypeUndef};
        req = data->requests.front();
        data->requests.pop();
        switch (req) {
            case EventTypeBarrierSync:
                data->dynamicTraceLogger.Write(&req);
                break;
            default:
                SINUCA3_ERROR_PRINTF("Request is invalid!\n");
                return;
        }
    }
    PIN_ReleaseLock(&threadRequestLock);
}

/** @brief Set up thread data */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&globalDataLock, tid + 1);

    global.staticTraceMetadata.threads++;
    ThreadData* data = new ThreadData;
    if (!PIN_SetThreadData(tlsKey, data, tid)) {
        SINUCA3_ERROR_PRINTF("PIN_SetThreadData failed\n");
        PIN_ExitProcess(1);
    }

    data->pathToDynFile =
        GetFormattedPath(global.traceDirectory.c_str(), "dynamic", tid);
    data->pathToMemFile =
        GetFormattedPath(global.traceDirectory.c_str(), "memory", tid);
    if (data->dynamicTraceLogger.Open(data->pathToDynFile.c_str()) ||
        data->memoryTraceLogger.Open(data->pathToMemFile.c_str())) {
        assert(false && "Invalid path\n");
    }

    PIN_ReleaseLock(&globalDataLock);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    ThreadData* data = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    assert(data != nullptr && "Thread data was not initialized!\n");

    FileHeader dynamicTraceHeader(FileTypeDynamicTrace, TargetArchX86);
    dynamicTraceHeader.Set(&data->dynamicTraceMetadata);
    data->dynamicTraceLogger.SetHeader(&dynamicTraceHeader);

    FileHeader memoryTraceHeader(FileTypeMemoryTrace, TargetArchX86);
    // set metadata here if ever necessary
    data->memoryTraceLogger.SetHeader(&memoryTraceHeader);

    delete data;
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(THREADID tid, UINT32 bbl, UINT64 size) {
    BasicBlockIdentifier id = static_cast<BasicBlockIdentifier>(bbl);

    PIN_GetLock(&globalDataLock, tid + 1);
    global.totalExecuted += static_cast<InstructionCounter>(size);
    InstructionCounter total = global.totalExecuted;
    PIN_ReleaseLock(&globalDataLock);
    if (total > knobNumberOfInstructions.Value()) {
        SINUCA3_WARNING_PRINTF("Reached maximum of instructions!\n");
        PIN_LockClient();
        for (int i = 0; i < global.staticTraceMetadata.threads; i++) {
            ThreadData* data =
                static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, i));
            assert(data != nullptr && "Invalid thread data!\n");
            EventType event = EventTypeAbruptEnd;
            data->dynamicTraceLogger.Write(&event);
        }
        PIN_UnlockClient();
        PIN_ExitApplication(0);
    }

    ThreadData* data = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    assert(data != nullptr && "Invalid thread data!\n");
    CheckForThreadRequests(tid, data);
    data->dynamicTraceLogger.Write(&id);
}

/** @brief Add memory operations to trace. */
VOID AppendToMemTrace(THREADID tid, PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    ThreadData* data = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    assert(data != nullptr && "Invalid thread data!\n");

    MemoryAccessType Load = MemoryAccessLoad;
    MemoryAccessType Store = MemoryAccessStore;

    int bits = accessInfo->numberOfMemops;

    MemoryAccessCounter count = static_cast<MemoryAccessCounter>(bits);
    for (int i = 0; i < bits; i++)
        if (!accessInfo->memop[i].maskOn) --count;
    data->memoryTraceLogger.Write(&count);

    for (int i = 0; i < bits; i++) {
        if (accessInfo->memop[i].maskOn) {
            auto& op = accessInfo->memop[i];
            assert(op.bytesAccessed <= MAX_MEM_ACCESS_SIZE);
            MemoryAccess acc{
                .address{op.memoryAddress},
                .typeOfAccess{op.memopType == PIN_MEMOP_LOAD ? Load : Store},
                .size{op.bytesAccessed}};
            data->memoryTraceLogger.Write(&acc);
        }
    }
}

VOID FetchRegisters(CompressedInstruction* inst, const INS& pinInst) {
    const auto rArraySize{std::size(inst->readRegs.regs)};
    const auto wArraySize{std::size(inst->writtenRegs.regs)};
    for (unsigned int i = 0; i < INS_OperandCount(pinInst); i++) {
        /* Interest only in register operands */
        if (!INS_OperandIsReg(pinInst, i)) {
            continue;
        }

        const auto reg{static_cast<unsigned short>(INS_OperandReg(pinInst, i))};

        const xed_decoded_inst_t* xed{INS_XedDec(pinInst)};

        auto isFp = [&xed, &inst](unsigned int idx) {
            xed_operand_element_type_enum_t type =
                xed_decoded_inst_operand_element_type(xed, idx);
            switch (type) {
                case XED_OPERAND_ELEMENT_TYPE_SINGLE:
                case XED_OPERAND_ELEMENT_TYPE_DOUBLE:
                case XED_OPERAND_ELEMENT_TYPE_LONGDOUBLE:
                case XED_OPERAND_ELEMENT_TYPE_FLOAT16:
                case XED_OPERAND_ELEMENT_TYPE_BFLOAT16:
                case XED_OPERAND_ELEMENT_TYPE_BFLOAT8:
                case XED_OPERAND_ELEMENT_TYPE_FLOAT8:
                case XED_OPERAND_ELEMENT_TYPE_HFLOAT8:
                    return 1;
                default:
                    return 0;
            }
        };

        if (INS_OperandRead(pinInst, i)) {
            if (inst->readRegs.occupation >= rArraySize) {
                SINUCA3_ERROR_PRINTF("Not enough registers!\n");
                assert(false);
            }
            inst->readRegs.regs[inst->readRegs.occupation].val = reg;
            inst->readRegs.regs[inst->readRegs.occupation].isFp = isFp(i);
            inst->readRegs.occupation++;
        }
        if (INS_OperandWritten(pinInst, i)) {
            if (inst->writtenRegs.occupation >= wArraySize) {
                SINUCA3_ERROR_PRINTF("Not enough registers!\n");
                assert(false);
            }
            inst->writtenRegs.regs[inst->writtenRegs.occupation].val = reg;
            inst->writtenRegs.regs[inst->writtenRegs.occupation].isFp = isFp(i);
            inst->writtenRegs.occupation++;
        }
    }
}

VOID TranslatePinInst(CompressedInstruction* inst, const INS& pinInst) {
    assert(inst != NULL);
    assert(pinInst != NULL);

    memset(inst, 0, sizeof(*inst));

    std::string mnemonic = INS_Mnemonic(pinInst);
    long size = sizeof(inst->instructionMnemonic) - 1;
    assert(size >= (long)mnemonic.size());
    strncpy(inst->instructionMnemonic, mnemonic.c_str(), size);

    inst->instructionAddress = INS_Address(pinInst);

    // at most 15 bytes len for now
    inst->instructionSize = INS_Size(pinInst);

    // manual flush or cache coherence induced flush
    inst->instCausesCacheLineFlush = INS_IsCacheLineFlush(pinInst);

    // false for any instruction which in practice is a system call
    inst->isCallInstruction = INS_IsCall(pinInst);
    inst->isSyscallInstruction = INS_IsSyscall(pinInst);
    inst->isRetInstruction = INS_IsRet(pinInst);
    inst->isSysretInstruction = INS_IsSysret(pinInst);
    // false for unconditional branches and calls
    inst->instHasFallthrough = INS_HasFallThrough(pinInst);
    inst->isBranchInstruction = INS_IsBranch(pinInst);
    inst->isIndirectCtrlFlowInst = INS_IsIndirectControlFlow(pinInst);

    // fields checked before reading from memory trace
    inst->instReadsMemory = INS_IsMemoryRead(pinInst);
    inst->instWritesMemory = INS_IsMemoryWrite(pinInst);

    // e.g. CMOV
    inst->isPredicatedInst = INS_IsPredicated(pinInst);

    // check if instruction is atomic
    inst->instPerformsAtomicUpdate = INS_IsAtomicUpdate(pinInst);

    FetchRegisters(inst, pinInst);
}

VOID OnTrace(TRACE trace, VOID* ptr) {
    THREADID tid = PIN_ThreadId();
    ThreadData* data = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    assert(data != nullptr && "Invalid thread data!\n");

    PIN_GetLock(&globalDataLock, tid + 1);
    if (!global.isInstrumenting || !data->isInstrumenting) {
        PIN_ReleaseLock(&globalDataLock);
        return;
    }

    RTN rtn = TRACE_Rtn(trace);
    if (!RTN_Valid(rtn)) {
        SINUCA3_ERROR_PRINTF("Found invalid routine! Skipping...\n");
        PIN_ReleaseLock(&globalDataLock);
        return;
    }

    RTN_Open(rtn);
    std::string routine = RTN_Name(rtn);
    RTN_Close(rtn);

    /* Remove unwanted spinlock. */
    for (auto& rtnToIgnore : global.ignore) {
        if (routine == rtnToIgnore) {
            SINUCA3_DEBUG_PRINTF("Ignoring [%s]!\n", routine.c_str());
            PIN_ReleaseLock(&globalDataLock);
            return;
        }
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BasicBlockSize size = static_cast<BasicBlockSize>(BBL_NumIns(bbl));
        global.staticTraceLogger.Write(&size);
        BasicBlockIdentifier idx = global.staticTraceMetadata.basicBlocks;
        global.staticTraceMetadata.basicBlocks++;

        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_THREAD_ID, IARG_UINT32, idx, IARG_UINT64, size,
                       IARG_END);

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            global.staticTraceMetadata.instructions++;
            CompressedInstruction compressed;
            IntrinsicInfo* intrinsic = GetIntrinsicInfo(&ins);
            bool isIntrinsic = (intrinsic != nullptr);

            if (isIntrinsic) {
                IntrinsicToSinucaInst(&ins, intrinsic, &compressed);
                global.staticTraceLogger.Write(&compressed);
                continue;
            }

            TranslatePinInst(&compressed, ins);
            global.staticTraceLogger.Write(&compressed);

            if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTrace,
                               IARG_THREAD_ID, IARG_MULTI_MEMORYACCESS_EA,
                               IARG_END);
            }
        }
    }

    PIN_ReleaseLock(&globalDataLock);
}

VOID OnExecutionEvent(THREADID tid, UINT32 ev, BOOL addToChildThr) {
    ThreadData* data = static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, tid));
    assert(data != nullptr && "Invalid thread data!\n");
    CheckForThreadRequests(tid, data);

    EventType event = static_cast<EventType>(ev);
    data->dynamicTraceLogger.Write(&event);

    if (addToChildThr) {
        PIN_GetLock(&globalDataLock, tid + 1);
        ThreadCounter threads = global.staticTraceMetadata.threads;
        PIN_ReleaseLock(&globalDataLock);
        for (ThreadCounter thr = 1; thr < threads; thr++) {
            PIN_GetLock(&threadRequestLock, tid + 1);
            ThreadData* data =
                static_cast<ThreadData*>(PIN_GetThreadData(tlsKey, thr));
            assert(data != nullptr && "Invalid thread data!\n");
            data->requests.push(event);
            PIN_ReleaseLock(&threadRequestLock);
        }
    }
}

VOID OnImageLoad(IMG img, VOID* ptr) {
    if (!IMG_IsMainExecutable(img)) return;
    THREADID tid = PIN_ThreadId();
    PIN_GetLock(&globalDataLock, tid + 1);

    std::string absoluteImgPath = IMG_Name(img);
    long idx = absoluteImgPath.find_last_of('/') + 1;
    std::string application = absoluteImgPath.substr(idx);

    if (!std::filesystem::exists(global.traceDirectory)) {
        std::filesystem::create_directory(global.traceDirectory);
    }

    global.traceDirectory = global.traceDirectory + "/" + application + "/";

    if (!std::filesystem::exists(global.traceDirectory)) {
        std::filesystem::create_directory(global.traceDirectory);
    }

    global.pathToStFile =
        GetFormattedPath(global.traceDirectory.c_str(), "static");
    if (global.staticTraceLogger.Open(global.pathToStFile.c_str()))
        assert(false && "Failed to open static file!\n");

    /* Only thread master run these calls. */
    std::vector<std::string> ompBarrierMasterStartVec;
    ompBarrierMasterStartVec.push_back("gomp_team_start");
    /* All threads run these calls. */
    std::vector<std::string> ompBarrierSimpleVec;
    ompBarrierSimpleVec.push_back("GOMP_loop_dynamic_start");
    ompBarrierSimpleVec.push_back("GOMP_barrier");
    ompBarrierSimpleVec.push_back("GOMP_loop_ordered_static_start");
    ompBarrierSimpleVec.push_back("GOMP_loop_guided_start");
    ompBarrierSimpleVec.push_back("GOMP_loop_end");
    ompBarrierSimpleVec.push_back("GOMP_parallel_sections_start");
    ompBarrierSimpleVec.push_back("GOMP_sections_end");
    /* Critical region begin. */
    std::vector<std::string> ompCriticalStartVec;
    ompCriticalStartVec.push_back("GOMP_atomic_start");
    ompCriticalStartVec.push_back("GOMP_critical_start");
    ompCriticalStartVec.push_back("GOMP_critical_name_start");
    /* Critical region end. */
    std::vector<std::string> ompCriticalEndVec;
    ompCriticalEndVec.push_back("GOMP_atomic_end");
    ompCriticalEndVec.push_back("GOMP_critical_end");
    ompCriticalEndVec.push_back("GOMP_critical_name_end");
    /* Routines to not instrument. */
    global.ignore.push_back("gomp_mutex_lock_slow");
    global.ignore.push_back("gomp_sem_wait_slow");
    global.ignore.push_back("gomp_ptrlock_get_slow");
    global.ignore.push_back("gomp_barrier_wait_end");
    global.ignore.push_back("pthread_mutex_lock");
    global.ignore.push_back("pthread_mutex_cond_lock");
    global.ignore.push_back("pthread_spinlock");
    global.ignore.push_back("pthread_mutex_timedlock");

    /* Instrumentation control. */
    std::string INSTRUMENTATION_START = "BeginInstrumentationBlock";
    std::string INSTRUMENTATION_END = "EndInstrumentationBlock";

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            if (!RTN_Valid(rtn)) continue;
            RTN_Open(rtn);

            std::string rtnName = RTN_Name(rtn);
            bool routineTreated{false};

            if (rtnName == INSTRUMENTATION_START) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InitInstrumentation,
                               IARG_END);
                routineTreated = true;
            } else if (rtnName == INSTRUMENTATION_END) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopInstrumentation,
                               IARG_END);
                routineTreated = true;
            }

            if (rtnName.compare(0, 4, "gomp") &&
                rtnName.compare(0, 4, "GOMP")) {
                RTN_Close(rtn);
                continue;
            }

            for (auto& barrierMaster : ompBarrierMasterStartVec) {
                if (routineTreated) break;
                if (rtnName == barrierMaster) {
                    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)OnExecutionEvent,
                                   IARG_THREAD_ID, IARG_UINT32,
                                   EventTypeBarrierSync, IARG_BOOL, true,
                                   IARG_END);
                    routineTreated = true;
                }
            }
            for (auto& barrierSimple : ompBarrierSimpleVec) {
                if (routineTreated) break;
                if (rtnName == barrierSimple) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)OnExecutionEvent, IARG_THREAD_ID,
                                   IARG_UINT32, EventTypeBarrierSync, IARG_BOOL,
                                   false, IARG_END);
                    routineTreated = true;
                }
            }
            for (auto& criticalStart : ompCriticalStartVec) {
                if (routineTreated) break;
                if (rtnName == criticalStart) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)OnExecutionEvent, IARG_THREAD_ID,
                                   IARG_UINT32, EventTypeCriticalStart,
                                   IARG_BOOL, false, IARG_END);
                    routineTreated = true;
                }
            }
            for (auto& criticalEnd : ompCriticalEndVec) {
                if (routineTreated) break;
                if (rtnName == criticalEnd) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)OnExecutionEvent, IARG_THREAD_ID,
                                   IARG_UINT32, EventTypeCriticalEnd, IARG_BOOL,
                                   false, IARG_END);
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

    PIN_ReleaseLock(&globalDataLock);
}

VOID OnFini(INT32 code, VOID* ptr) {
    FileHeader staticTraceHeader(FileTypeStaticTrace, TargetArchX86);
    staticTraceHeader.Set(&global.staticTraceMetadata);
    global.staticTraceLogger.SetHeader(&staticTraceHeader);

#ifndef NDEBUG
    SINUCA3_LOG_PRINTF("Header from static trace: \n");
    staticTraceHeader.Print(true);
#endif

    if (!global.hasInitiatedInstrumenting) {
        SINUCA3_WARNING_PRINTF(
            "No instrumentation blocks were found in the target program!\n\n");
    }
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    global.traceDirectory = knobFolder.Value();
    if (global.traceDirectory.back() == '/') {
        global.traceDirectory.pop_back();
    }

    PIN_InitLock(&threadRequestLock);
    PIN_InitLock(&globalDataLock);
    tlsKey = PIN_CreateThreadDataKey(0);

    if (knobForceInstrumentation.Value()) {
        SINUCA3_WARNING_PRINTF("Instrumenting entire program\n");
        InitInstrumentation();
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
