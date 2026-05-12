#ifndef SINUCA3_ENGINE_HPP_
#define SINUCA3_ENGINE_HPP_

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
 * @file engine.hpp
 * @brief Public API of the simulation engine.
 */

#include <ctime>
#include <engine/build_definitions.hpp>
#include <engine/component.hpp>
#include <tracer/trace_reader.hpp>

#include "engine/default_packets.hpp"
#include "utils/map.hpp"

extern int totalCores;

int NewComponentDefinition(Map<Definition>* definitions,
                           Map<Linkable*>* aliases,
                           std::vector<InstanceWithDefinition>* instances,
                           Map<yaml::YamlValue>* config, const char* name,
                           const char* alias, yaml::YamlLocation location);

class AddressMapper {
  public:
    AddressMapper() {}
    bool GetAddressMapping(unsigned long address, int context, unsigned long* mappedAddress) {
        unsigned long contextFrame = context;
        unsigned long addressMask = (1UL << 48) - 1;
        *mappedAddress = (address & addressMask) | (contextFrame << 48);
        return true;
    }
};

struct FetchBuffer {
    InstructionPacket currPkt;
    InstructionPacket nextPkt;
    long bytesRequested;
    bool isWaiting;
    bool isCurrentFetched;
    bool isCurrentValid;
    bool hasError;
    bool reachedEnd;

    TraceReader* tracer;
    int tid;

    FetchBuffer()
        : bytesRequested(0),
          isWaiting(false),
          isCurrentFetched(false),
          isCurrentValid(false),
          hasError(false),
          reachedEnd(false),
          tracer(NULL),
          tid(-1) {}

    /** @brief Returns the number of instructions to be fetched. */
    unsigned long GetInstToBeFetched();
    /** @brief Sets the trace reader and thread id. */
    void SetTracerAndTid(TraceReader* tracer, int tid);
    /** @brief Copy current (instruction) to target and update current. */
    void GetPkt(InstructionPacket* target);
    /** @brief Tries to fetch an instruction. */
    void TryFetch();
    /** @brief Remembers the request from fetcher. */
    void RememberRequest(unsigned long request);
    /** @brief Clears the saved request. */
    void ClearRequest();
    /** @brief Checks if the fetche buffer is properly set. */
    inline bool IsValid() { return (this->tracer != NULL && this->tid >= 0); }
    /** @brief Checks if the current instruction is ready. */
    inline bool IsReady() { return (this->IsValid() && this->isCurrentValid); }
    /** @brief Checks if fetcher has requested instructions. */
    inline bool HasFetcherRequested() { return this->isWaiting; }
    /** @brief Returns the number of bytes requested. */
    inline long GetBytesRequested() { return this->bytesRequested; }
};


/**
 * @brief The engine itself.
 * @details A component may fetch an instruction by sending a message to the
 * engine. In the configuration file, it's accessible via the pre-defined alias
 * *ENGINE. Each connection to the engine represents a core.
 */
class Engine : public Component<FetchPacket> {
  private:
    Linkable**
        components; /** @brief The components of the simulation INCLUDING
                       THE ENGINE ITSELF, guaranteed to be the first element. */
    long numberOfComponents; /** @brief The number of components. */
    long numberOfFetchers; /** @brief The number of components connected to the
                              engine. I.e., cores. */
    unsigned long totalCycles; /** @brief Counter of cycles. */
    unsigned long
        fetchedInstructions; /** @brief Counter of instructions fetched. */
    unsigned long traceSize; /** @brief The total amount of instructions to be
                                executed. */

    /** @brief Will be set when there's no more instructions in the trace. */
    bool end;
    /** @brief Will be set if the traceReader returns an error. */
    bool error;

    FetchBuffer* fetchBuffers;  /** @brief Instruction buffer managers. */
    AddressMapper* addressMapper;

    /**
     * @brief Returns the number of instructions to be executed.
     */
    unsigned long GetTraceSize();

    /**
     * @brief Prints the estimated remaining simulation time.
     */
    void PrintTime(time_t start, unsigned long cycle);

    /** @brief Called at the beggining of Simulate(). */
    int SetupSimulation(std::vector<TraceReader*>* traceReaders);

    /** @brief Auxiliar to Fetch(). */
    int SendBufferedAndFetch(int id);

    /** @brief Responds to requests. */
    void Fetch(int id);

    /** @brief Respond setup request for context. */
    void SendContextToCore(int id);

  public:
    inline Engine()
        : components(NULL),
          numberOfComponents(0),
          numberOfFetchers(0),
          totalCycles(0),
          fetchedInstructions(0),
          end(false),
          error(false),
          fetchBuffers(NULL) {}

    /** @brief Instantiates a simulation from the array of components. */
    inline void Instantiate(Linkable** components, long numberOfComponents) {
        this->components = components;
        this->numberOfComponents = numberOfComponents;
    }

    /**
     * @brief Self-explanatory.
     * @returns Non-zero if the simulation stopped because of a problem. 0 if it
     * stopped normally.
     */
    int Simulate(std::vector<TraceReader*>* tracers);

    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();

    virtual ~Engine();
};

#endif  // SINUCA3_ENGINE_HPP_