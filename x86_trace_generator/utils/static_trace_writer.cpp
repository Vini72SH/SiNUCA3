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
 * @file static_trace_writer.cpp
 * @details Implementation of StaticTraceFile class.
 */

#include "static_trace_writer.hpp"

#include <cassert>
#include <cstdio>
#include <sinuca3.hpp>
#include <string>

#include "tracer/sinuca/file_handler.hpp"

extern "C" {
#include <alloca.h>
}

int sinucaTracer::StaticTraceFile::OpenFile(const char* sourceDir,
                                            const char* imgName) {
    unsigned long bufferSize;
    char* path;

    bufferSize = sinucaTracer::GetPathTidOutSize(sourceDir, "static", imgName);
    path = (char*)alloca(bufferSize);
    FormatPathTidOut(path, sourceDir, "static", imgName, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) return 1;
    /* This space will be used to store the file header. */
    fseek(this->file, sizeof(this->header), SEEK_SET);
    return 0;
}

int sinucaTracer::StaticTraceFile::AllocBasicBlock() {
    this->basicBlockInstCapacity = 100; /* arbitrary choice */
    this->basicBlock = (BasicBlock*)malloc(
        sizeof(BasicBlock) + basicBlockInstCapacity * sizeof(Instruction));
    return basicBlock == NULL;
}

int sinucaTracer::StaticTraceFile::ReallocBasicBlock(unsigned long cap) {
    this->basicBlockInstCapacity = cap;
    this->basicBlock = (BasicBlock*)realloc(
        this->basicBlock,
        sizeof(BasicBlock) + basicBlockInstCapacity * sizeof(Instruction));
    return basicBlock == NULL;
}

void sinucaTracer::StaticTraceFile::InitializeHeader() {
    this->header.data.staticHeader.bblCount = 0;
    this->header.data.staticHeader.instCount = 0;
    this->header.data.staticHeader.threadCount = 0;
}

int sinucaTracer::StaticTraceFile::WriteBasicBlockToFile() {
    if (this->file == NULL) return 1;
    if (this->basicBlock == NULL) return 1;

    unsigned long written;
    written =
        fwrite(this->basicBlock, sizeof(*this->basicBlock), 1, this->file);
    if (written != sizeof(*this->basicBlock)) return 1;
    written = fwrite(this->basicBlock->instructions,
                     sizeof(Instruction) * this->basicBlockSize, 1, this->file);
    if (written != sizeof(Instruction) * this->basicBlockSize) return 1;
    this->instIndex = 0;
    return 0;
}

int sinucaTracer::StaticTraceFile::WriteHeaderToFile() {
    if (this->file == NULL) return 1;

    unsigned long written;
    rewind(this->file);
    written = fwrite(&this->header, sizeof(this->header), 1, this->file);
    return written != sizeof(this->header);
}

void sinucaTracer::StaticTraceFile::AddInstructionToBasicBlock(
    const INS* pinInstruction) {
    if (basicBlockInstCapacity == 0) {
        if (this->AllocBasicBlock()) return 1;
    }
    Instruction* rawInst = &basicBlock->instructions[this->instIndex++];

    /* fill instruction name */
    std::string insName = INS_Mnemonic(*ins);
    unsigned long nameSize = insName.size();
    if (nameSize >= MAX_INSTRUCTION_NAME_LENGTH) {
        nameSize = MAX_INSTRUCTION_NAME_LENGTH - 1;
    }
    memcpy(rawInst->name, insName.c_str(), nameSize);
    rawInst->name[nameSize] = '\0';
    rawInst->addr = INS_Address(*ins);
    rawInst->size = INS_Size(*ins);
    rawInst->baseReg = INS_MemoryBaseReg(*ins);
    rawInst->indexReg = INS_MemoryIndexReg(*ins);
    /* fill single bit fields */
    rawInst->isPredicated = (INS_IsPredicated(*ins)) ? 1 : 0;
    rawInst->isPrefetch = (INS_IsPrefetch(*ins)) ? 1 : 0;
    rawInst->isNonStandardMemOp = (!INS_IsStandardMemop(*ins)) ? 1 : 0;
    if (!rawInst->isNonStandardMemOp) {
        rawInst->numStdMemReadOps = (INS_IsMemoryRead(*ins)) ? 1 : 0;
        rawInst->numStdMemReadOps += INS_HasMemoryRead2(*ins) ? 1 : 0;
        rawInst->numStdMemWriteOps = INS_IsMemoryWrite(*ins) ? 1 : 0;
    }
    /* fill branch related fields */
    bool isSyscall = INS_IsSyscall(*ins);
    rawInst->isControlFlow = (INS_IsControlFlow(*ins) || isSyscall) ? 1 : 0;
    if (rawInst->isControlFlow) {
        if (isSyscall)
            this->data.branchType = BRANCH_SYSCALL;
        else if (INS_IsCall(*ins))
            this->data.branchType = BRANCH_CALL;
        else if (INS_IsRet(*ins))
            this->data.branchType = BRANCH_RETURN;
        else if (INS_HasFallThrough(*ins))
            this->data.branchType = BRANCH_COND;
        else
            this->data.branchType = BRANCH_UNCOND;
        rawInst->isIndirectControlFlow =
            INS_IsIndirectControlFlow(*ins) ? 1 : 0;
    }
    /* fill used register info */
    unsigned int operandCount = INS_OperandCount(*ins);
    rawInst->numReadRegs = rawInst->numWriteRegs = 0;
    for (unsigned int i = 0; i < operandCount; ++i) {
        if (!INS_OperandIsReg(*ins, i)) continue;
        if (INS_OperandWritten(*ins, i)) {
            if (this->data.numWriteRegs > MAX_REG_OPERANDS) return;
            this->data.writeRegs[this->data.numWriteRegs++] =
                INS_OperandReg(*ins, i);
        }
        if (INS_OperandRead(*ins, i)) {
            if (this->data.numReadRegs > MAX_REG_OPERANDS) return;
            this->data.readRegs[this->data.numReadRegs++] =
                INS_OperandReg(*ins, i);
        }
    }
}