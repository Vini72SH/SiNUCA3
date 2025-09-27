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
 * @file x86_reader_file_handler.cpp
 * @brief Implementation of the x86_64 trace reader.
 */

#include "static_trace_reader.hpp"

#include <cstring>

#include <tracer/sinuca/file_handler.hpp>
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap
#include <unistd.h>    // lseek
}

int sinucaTracer::StaticTraceFile::OpenFile(const char *sourceDir,
                                            const char *imgName) {
    unsigned long bufferLen;
    char *staticPath;

    bufferLen = GetPathTidOutSize(sourceDir, "static", imgName);
    staticPath = (char *)alloca(bufferLen);
    FormatPathTidOut(staticPath, sourceDir, "static", imgName, bufferLen);

    /* get file descriptor */
    this->fileDescriptor = open(staticPath, O_RDONLY);
    if (fileDescriptor == -1) {
        printFileErrorLog(staticPath, "O_RDONLY");
        return 1;
    }
    /* get file size */
    this->mmapSize = lseek(this->fileDescriptor, 0, SEEK_END);
    SINUCA3_DEBUG_PRINTF("Mmap Size [%lu]\n", this->mmapSize);
    /* map file to virtual memory */
    this->mmapPtr = (char *)mmap(0, this->mmapSize, PROT_READ, MAP_PRIVATE,
                                 this->fileDescriptor, 0);
    if (this->mmapPtr == MAP_FAILED) {
        printFileErrorLog(staticPath, "PROT_READ MAP_PRIVATE");
        return 1;
    }

    return 0;
}

sinucaTracer::StaticTraceFile::~StaticTraceFile() {
    if (this->mmapPtr == NULL) return;
    munmap(this->mmapPtr, this->mmapSize);
    if (this->fileDescriptor == -1) return;
    close(this->fileDescriptor);
}

void *sinucaTracer::StaticTraceFile::ReadData(unsigned long len) {
    if (this->mmapOffset >= this->mmapSize) return NULL;
    void *ptr = (void *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += len;
    return ptr;
}

int sinucaTracer::StaticTraceFile::ReadFileHeader() {
    if (this->fileDescriptor == -1) return 1;
    void *readData = this->ReadData(sizeof(FileHeader));
    if (readData != this->mmapPtr) {
        SINUCA3_WARNING_PRINTF("Static trace header ptr seem wrong\n");
    }
    if (readData == NULL) return 1;
    this->header = *(FileHeader *)(readData);
    return 0;
}

int sinucaTracer::StaticTraceFile::ReadStaticRecordFromFile() {
    if (this->fileDescriptor == -1) return 1;
    void *readData;
    readData = this->ReadData(sizeof(this->record));
    if (readData == NULL) return 1;
    this->record = *(StaticRecord *)readData;
    return 0;
}

void sinucaTracer::StaticTraceFile::ConvertRawInstToSinucaInstFormat(
    InstructionInfo *instInfo, Instruction *rawInst) {
    /* info ignored if inst performs non std mem access */
    instInfo->staticNumReadings = rawInst->numStdMemWriteOps;
    instInfo->staticNumWritings = rawInst->numStdMemReadOps;

    /* copy inst name */
    strncpy(instInfo->staticInfo.opcodeAssembly, rawInst->name,
            TRACE_LINE_SIZE - 1);

    /* copy registers used */
    instInfo->staticInfo.numReadRegs = rawInst->numReadRegs;
    instInfo->staticInfo.numWriteRegs = rawInst->numWriteRegs;
    memcpy(instInfo->staticInfo.readRegs, rawInst->readRegs,
           sizeof(rawInst->readRegs));
    memcpy(instInfo->staticInfo.writeRegs, rawInst->writeRegs,
           sizeof(rawInst->writeRegs));

    instInfo->staticInfo.opcodeSize = rawInst->size;
    instInfo->staticInfo.baseReg = rawInst->baseReg;
    instInfo->staticInfo.indexReg = rawInst->indexReg;
    instInfo->staticInfo.opcodeAddress = rawInst->addr;

    /* single bit fields */
    instInfo->staticInfo.isNonStdMemOp =
        static_cast<bool>(rawInst->isNonStandardMemOp);
    instInfo->staticInfo.isControlFlow =
        static_cast<bool>(rawInst->isControlFlow);
    instInfo->staticInfo.isPredicated =
        static_cast<bool>(rawInst->isPredicated);
    instInfo->staticInfo.isPrefetch = static_cast<bool>(rawInst->isPrefetch);
    instInfo->staticInfo.isIndirect =
        static_cast<bool>(rawInst->isIndirectControlFlow);

    /* convert branch type to enum Branch */
    switch (rawInst->branchType) {
        case BRANCH_CALL:
            instInfo->staticInfo.branchType = BranchCall;
            break;
        case BRANCH_SYSCALL:
            instInfo->staticInfo.branchType = BranchSyscall;
            break;
        case BRANCH_RETURN:
            instInfo->staticInfo.branchType = BranchReturn;
            break;
        case BRANCH_COND:
            instInfo->staticInfo.branchType = BranchCond;
            break;
        case BRANCH_UNCOND:
            instInfo->staticInfo.branchType = BranchUncond;
            break;
    }
}