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
 * @file trace_reader.cpp
 */

#include <cassert>
#include <tracer/sinuca/trace_reader.hpp>

#include "file_handler.hpp"

FetchResult SinucaTraceReader::Fetch(InstructionPacket* ret, int tid) {
    if (this->HasExecutionEnded()) {
        return FetchResultEnd;
    }
    if (this->IsThreadSleeping(tid)) {
        if (!this->TryToWakeUpThread(tid)) return FetchResultWait;
    }
    if (!this->IsThreadInsideBasicBlock(tid)) {
        if (this->TryToFetchNewBasicBlock(tid)) return FetchResultWait;
    }

    this->FetchInstruction(ret, tid);

    return FetchResultOk;
}

int SinucaTraceReader::OpenTrace(const char* dir) {
    char* directory = this->FormatDirectory(dir);

    if (this->OpenInstructionsLog(directory)) {
        SINUCA3_ERROR_PRINTF("Failed to open instructions log!\n");
        return 1;
    }
    if (this->ReadMetadata()) {
        SINUCA3_ERROR_PRINTF("Failed to read trace metadata!\n");
        return 1;
    }
    if (this->CreateThreads(directory)) {
        SINUCA3_ERROR_PRINTF("Failed to create threads!\n");
        return 1;
    }
    if (this->VerifyVersionAndTarget()) {
        SINUCA3_ERROR_PRINTF(
            "Inconsistent version and target architecture detected!\n");
        return 1;
    }
    if (this->GenerateDictionaryOfInstructions()) {
        SINUCA3_ERROR_PRINTF("Failed to create dictionary of instructions!\n");
        return 1;
    }

#ifndef NDEBUG
    this->PrintHeaders();
#endif

    delete[] directory;

    return 0;
}

void SinucaTraceReader::PrintHeaders() {
    SINUCA3_LOG_PRINTF("Trace Headers:\n");
    for (int i = 0; i < this->threadCount; i++) {
        SINUCA3_LOG_PRINTF("\t Thread [%d]:\n", i);
        FileHeader header;
        SINUCA3_LOG_PRINTF("Header from dynamic file: \n");
        this->threads[i].executionLoader.GetHeader(&header);
        header.Print(true);
        SINUCA3_LOG_PRINTF("Header from memory file: \n");
        this->threads[i].memoryLoader.GetHeader(&header);
        header.Print(true);
    }
    FileHeader header;
    SINUCA3_LOG_PRINTF("Header from statics file: \n");
    this->instructionsLoader.GetHeader(&header);
    header.Print(true);
}

void SinucaTraceReader::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Trace Statistics:\n");
    SINUCA3_LOG_PRINTF("\t Barriers reached: [%d]\n", this->barriers);
    SINUCA3_LOG_PRINTF("\t Critical sections reached: [%d]\n",
                       this->criticalSections);
}

long SinucaTraceReader::GetNumberOfFetchedInst(int tid) {
    return this->threads[tid].fetchCount;
}

long SinucaTraceReader::GetTotalInstToBeFetched(int tid) {
    FileHeader header;
    this->threads[tid].executionLoader.GetHeader(&header);
    DynamicFileMetadata meta;
    header.Get(&meta);
    return meta.executed;
}

int SinucaTraceReader::GetTotalThreads() {
    FileHeader header;
    this->instructionsLoader.GetHeader(&header);
    StaticFileMetadata meta;
    header.Get(&meta);
    return meta.threads;
}

int SinucaTraceReader::GetTotalBasicBlocks() {
    FileHeader header;
    this->instructionsLoader.GetHeader(&header);
    StaticFileMetadata meta;
    header.Get(&meta);
    return meta.basicBlocks;
}

int Thread::Setup(const char* dir, int tid) {
    assert(dir != NULL);

    this->SetTid(tid);

    if (this->OpenExecutionLog(dir)) {
        SINUCA3_ERROR_PRINTF("Failed to open execution log for thread [%d]!\n",
                             tid);
        return 1;
    }
    if (this->OpenMemoryLog(dir)) {
        SINUCA3_ERROR_PRINTF("Failed to open memory log for thread [%d]!\n",
                             tid);
        return 1;
    }

    return 0;
}

void Thread::SetBasicBlock(int bbl, int size) {
    this->currentBasicBlock = bbl;
    this->currentInstruction = 0;
    this->currentBblSize = size;
    this->needToFetchNewBasicBlock = false;
}

void Thread::StepInstruction() {
    this->currentInstruction++;
    this->fetchCount++;
    if (this->currentInstruction >= this->currentBblSize) {
        this->needToFetchNewBasicBlock = true;
    }
}

int Thread::OpenExecutionLog(const char* directory) {
    this->pathToDynamicTrace = GetFormattedPath(directory, "dynamic", this->tid);
    assert(this->pathToDynamicTrace != NULL);
    return (this->executionLoader.Open(this->pathToDynamicTrace));
}

int Thread::OpenMemoryLog(const char* directory) {
    this->pathToMemoryTrace = GetFormattedPath(directory, "memory", this->tid);
    assert(this->pathToMemoryTrace != NULL);
    return (this->memoryLoader.Open(this->pathToMemoryTrace));
}

Thread::~Thread() {
    if (this->pathToDynamicTrace != NULL) {
        delete[] this->pathToDynamicTrace;
        this->pathToDynamicTrace = NULL;
    }
    if (this->pathToMemoryTrace != NULL) {
        delete[] this->pathToMemoryTrace;
        this->pathToMemoryTrace = NULL;
    }
}

SinucaTraceReader::~SinucaTraceReader() {
    if (this->threads != NULL) {
        delete[] this->threads;
        this->threads = NULL;
    }
    if (this->dictionary != NULL) {
        delete[] this->dictionary;
        this->dictionary = NULL;
    }
    if (this->pool != NULL) {
        delete[] this->pool;
        this->pool = NULL;
    }
    if (this->pathToStaticFile != NULL) {
        delete[] this->pathToStaticFile;
        this->pathToStaticFile = NULL;
    }
}

char* SinucaTraceReader::FormatDirectory(const char* directory) {
    assert(directory != NULL);
    long size = strlen(directory) + 2;
    char* formatted = new char[size];

    if (directory[strlen(directory) - 1] == '/') {
        snprintf(formatted, size, "%s", directory);
    } else {
        snprintf(formatted, size, "%s/", directory);
    }

    return formatted;
}

int SinucaTraceReader::ReadMetadata() {
    FileHeader header;
    if (this->instructionsLoader.GetHeader(&header)) {
        SINUCA3_ERROR_PRINTF("Failed to read instructions log header!\n");
        return 1;
    }
    StaticFileMetadata meta;
    if (header.Get(&meta)) {
        SINUCA3_ERROR_PRINTF("Failed to read instructions log metadata!\n");
        return 1;
    }
    this->threadCount = meta.threads;
    this->bblocksCount = meta.basicBlocks;
    this->recordedInst = meta.instructions;

    return 0;
}

int SinucaTraceReader::OpenInstructionsLog(const char* directory) {
    this->pathToStaticFile = GetFormattedPath(directory, "static");
    assert(this->pathToStaticFile != NULL);
    return (this->instructionsLoader.Open(this->pathToStaticFile));
}

int SinucaTraceReader::CreateThreads(const char* directory) {
    assert(directory != NULL);
    assert(this->threadCount > 0);

    this->threads = new Thread[this->threadCount];

    for (int i = 0; i < this->threadCount; i++) {
        if (this->threads[i].Setup(directory, i)) {
            SINUCA3_ERROR_PRINTF("Failed to open thread [%d]!\n", i);
            return 1;
        }
    }

    return 0;
}

int SinucaTraceReader::GenerateDictionaryOfInstructions() {
    assert(this->bblocksCount > 0);
    assert(this->recordedInst > 0);

    this->pool = new StaticInstructionInfo[this->recordedInst];
    this->dictionary = new BasicBlock[this->bblocksCount];

    long instructionIndex = 0;

    for (int bbIdx = 0; bbIdx < this->bblocksCount; bbIdx++) {
        BasicBlockSize bbSize = 0;
        this->instructionsLoader.Read(&bbSize);

        this->dictionary[bbIdx].size = bbSize;
        this->dictionary[bbIdx].instructions = &this->pool[instructionIndex];

        for (long instIdx = 0; instIdx < bbSize;
             instIdx++, instructionIndex++) {
            CompressedInstruction compressed;
            assert(instructionIndex < this->recordedInst);
            this->instructionsLoader.Read(&compressed);

            CompressedInstToStaticInfo(&compressed,
                                       &this->pool[instructionIndex]);
        }
    }

    return 0;
}

int SinucaTraceReader::VerifyVersionAndTarget() {
    assert(this->threads != NULL);

    FileHeader referenceHeader;
    this->instructionsLoader.GetHeader(&referenceHeader);

    for (int i = 0; i < this->threadCount; i++) {
        FileHeader header;

        this->threads[i].executionLoader.GetHeader(&header);
        if (header.version != referenceHeader.version) {
            SINUCA3_ERROR_PRINTF("Version mismatch detected in thread [%d]!\n",
                                 i);
            return 1;
        }
        if (header.target != referenceHeader.target) {
            SINUCA3_ERROR_PRINTF(
                "Target architecture mismatch detected in thread [%d]!\n", i);
            return 1;
        }

        this->threads[i].memoryLoader.GetHeader(&header);
        if (header.version != referenceHeader.version) {
            SINUCA3_ERROR_PRINTF("Version mismatch detected in thread [%d]!\n",
                                 i);
            return 1;
        }
        if (header.target != referenceHeader.target) {
            SINUCA3_ERROR_PRINTF(
                "Target architecture mismatch detected in thread [%d]!\n", i);
            return 1;
        }
    }

    return 0;
}

int SinucaTraceReader::IsThreadSleeping(int tid) {
    assert(tid >= 0 && tid < this->threadCount);
    return this->threads[tid].IsThreadWaiting();
}

int SinucaTraceReader::IsThreadInsideBasicBlock(int tid) {
    assert(tid >= 0 && tid < this->threadCount);
    return !this->threads[tid].NeedToFetchNewBasicBlock();
}

int SinucaTraceReader::TryToFetchNewBasicBlock(int tid) {
    assert(tid >= 0 && tid < this->threadCount);

    DynamicTraceEntry entry;
    BasicBlockIdentifier bbl;
    EventType event;

    if (this->threads[tid].executionLoader.Read(&entry)) {
        int eof = this->threads[tid].executionLoader.EndOfFile();
        this->threads[tid].SetHasEndedExecution(eof);
        return 1;
    }
    if (entry.Get(&bbl) == 0) {
        assert(bbl >= 0 && bbl < this->bblocksCount);
        this->threads[tid].SetBasicBlock(bbl, this->dictionary[bbl].size);
        return 0;
    }
    if (entry.Get(&event) == 0) {
        assert(IsValidEventType(event));
        this->HandleEvent(tid, event);
        return 1;
    }

    SINUCA3_ERROR_PRINTF("Unknown entry type [%d].\n", entry.type);

    return 1;
}

void SinucaTraceReader::HandleEvent(int tid, EventType event) {
    assert(tid >= 0 && tid < this->threadCount);

    switch (event) {
        case EventTypeBarrierSync:
            this->HandleBarrierSync(tid);
            break;

        case EventTypeCriticalStart:
            this->HandleCriticalStart(tid);
            break;

        case EventTypeCriticalEnd:
            this->HandleCriticalEnd(tid);
            break;

        case EventTypeAbruptEnd:
            this->HandleAbruptEnd();
            break;

        default:
            SINUCA3_WARNING_PRINTF("Unknown event type: %d.\n", event);
            break;
    }
}

bool SinucaTraceReader::TryToWakeUpThread(int tid) {
    assert(tid >= 0 && tid < this->threadCount);

    if (this->threads[tid].isWaitingForCriticalSection) {
        for (int i = 0; i < this->threadCount; i++) {
            if (i != tid && this->threads[i].isInsideCriticalSection) return false;
        }
        this->threads[tid].SetInsideCriticalSection();
    } else if (this->threads[tid].waitingAtBarrier) {
        for (int i = 0; i < this->threadCount; i++) {
            if (i != tid && !this->threads[i].waitingAtBarrier) return false;
        }
        for (int i = 0; i < this->threadCount; i++) {
            this->threads[i].SetLeftBarrier();
        }
    }

    return true;
}

int SinucaTraceReader::PerformsMemoryAccess(const StaticInstructionInfo* inst) {
    assert(inst != NULL);
    return inst->instReadsMemory || inst->instWritesMemory;
}

void SinucaTraceReader::FillEmptyAccess(InstructionPacket* ret) {
    assert(ret != NULL);
    ret->dynamicInfo.numReadings = 0;
    ret->dynamicInfo.numWritings = 0;
}

void SinucaTraceReader::FetchMemoryAccess(InstructionPacket* ret, int tid) {
    assert(ret != NULL);
    assert(tid >= 0 && tid < this->threadCount);

    MemoryAccessCounter accessCount = -1;
    this->threads[tid].memoryLoader.Read(&accessCount);
    assert(accessCount >= 0 && accessCount <= MAX_MEM_OPERATIONS);

    ret->dynamicInfo.numReadings = 0;
    ret->dynamicInfo.numWritings = 0;

    for (int i = 0; i < accessCount; i++) {
        MemoryAccess access;
        this->threads[tid].memoryLoader.Read(&access);

        assert(IsValidMemoryAccessType(access.typeOfAccess));

        if (access.typeOfAccess == MemoryAccessLoad) {
            ret->dynamicInfo.readsAddr[ret->dynamicInfo.numReadings] =
                access.address;
            ret->dynamicInfo.readsSize[ret->dynamicInfo.numReadings] =
                access.size;
            ret->dynamicInfo.numReadings++;
        } else if (access.typeOfAccess == MemoryAccessStore) {
            ret->dynamicInfo.writesAddr[ret->dynamicInfo.numWritings] =
                access.address;
            ret->dynamicInfo.writesSize[ret->dynamicInfo.numWritings] =
                access.size;
            ret->dynamicInfo.numWritings++;
        }
    }
}

void SinucaTraceReader::FetchInstruction(InstructionPacket* ret, int tid) {
    assert(tid >= 0 && tid < this->threadCount);
    assert(ret != NULL);

    ret->staticInfo = &this->dictionary[this->threads[tid].currentBasicBlock]
                           .instructions[this->threads[tid].currentInstruction];

    if (this->PerformsMemoryAccess(ret->staticInfo)) {
        this->FetchMemoryAccess(ret, tid);
    } else {
        this->FillEmptyAccess(ret);
    }

    this->threads[tid].StepInstruction();
}

int SinucaTraceReader::HasExecutionEnded() {
    assert(this->threads != NULL);

    for (int i = 0; i < this->threadCount; i++) {
        if (!this->threads[i].HasThreadEndedExecution()) {
            return 0;
        }
    }

    return 1;
}

void SinucaTraceReader::HandleBarrierSync(int tid) {
    assert(tid >= 0 && tid < this->threadCount);
    assert(!this->threads[tid].IsThreadWaiting());
    this->threads[tid].SetArrivedAtBarrier();
    this->barriers++;
}

void SinucaTraceReader::HandleCriticalStart(int tid) {
    assert(tid >= 0 && tid < this->threadCount);
    assert(!this->threads[tid].IsThreadWaiting());
    this->threads[tid].SetWaitingForCriticalSection();
    this->criticalSections++;
}

void SinucaTraceReader::HandleCriticalEnd(int tid) {
    assert(tid >= 0 && tid < this->threadCount);
    assert(!this->threads[tid].IsThreadWaiting());
    this->threads[tid].SetExitedCriticalSection();
    this->TryToWakeUpThread(tid);
}

void SinucaTraceReader::HandleAbruptEnd() {
    for (int i = 0; i < this->threadCount; i++)
        this->threads[i].SetHasEndedExecution(true);
}
