#ifndef SINUCA3_X86_TRACE_READER_HPP_
#define SINUCA3_X86_TRACE_READER_HPP_

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
 * @file x86_trace_reader.hpp
 * @brief Implementation of SinucaTraceReader for x86 based traces
 */

#include "../trace_reader.hpp"
#include "utils/dynamic_trace_reader.hpp"
#include "utils/memory_trace_reader.hpp"
#include "utils/static_trace_reader.hpp"

namespace tracer {

struct ThrInfo {
    DynamicTraceFile *dynFile;
    MemoryTraceFile *memFile;
    unsigned int currentBBL;    /**<Index of basic block. */
    unsigned int currentOpcode; /**<Index of instruction inside basic block. */
    bool isInsideBBL;

  public:
    int Allocate(const char *sourceDir, const char *imageName, int tid);
    ~ThrInfo();
};

class SinucaTraceReader : public TraceReader {
  private:
    ThrInfo *thrsInfo; /**<Information specific to each thread. */

    int numThreads;

    unsigned int binaryTotalBBLs; /**<Number of basic blocks in static file. */
    unsigned int *binaryBBLsSize; /**<Number of instructions per basic block. */
    InstructionInfo **binaryDict; /**<Array containing all instructions. */
    InstructionInfo *pool;        /**<Pool used for more efficient allocation.*/

    unsigned long fetchInstructions; /**<Number of fetched instructions */

    /**
     * @brief Fill instructions dictionary
     * @details Info per instruction:
     * Address        | Ins. Size   | Base Reg    | Index Reg       |
     * Is predicated  | Is prefetch | Is indirect | Is !std mem op  |
     * Is read        | Has read2   | Is write    | Num. Write Regs |
     * Num. Read Regs | Read Regs   | Write Regs  | Ins. Mnemonic   |
     * Branch Type
     */
    void GenerateBinaryDict(StaticTraceFile *);

  public:
    /**
     * @brief Class attributes initializer.
     * @param imageName Name of the executable used to generate the traces.
     * @param sourceDir Complete path to the directory that stores the traces.
     */
    virtual int OpenTrace(const char *imageName, const char *sourceDir);
    /**
     * @brief Prints the number of fetched instructions for now.
     */
    virtual void PrintStatistics();
    /**
     * @brief Trace size is the equivalent to the number of basic blocks from
     * static trace.
     */
    virtual unsigned long GetTraceSize();
    /**
     * @return Value stored in fetchInstructions.
     */
    virtual unsigned long GetNumberOfFetchedInstructions();
    /**
     * @brief Get next executed instruction.
     * @param ret Pointer to struct that will be filled.
     * @param tid Thread identifier.
     */
    virtual FetchResult Fetch(InstructionPacket *ret, unsigned int tid);
    /**
     * @brief Free allocated memory in OpenTrace and GenerateBinaryDict.
     */
    void CloseTrace();
    inline ~SinucaTraceReader() { this->CloseTrace(); }
};

}  // namespace tracer

#endif  // SINUCA3_X86_TRACE_READER_HPP_
