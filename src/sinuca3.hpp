#ifndef SINUCA3_HPP_

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
 * @file sinuca3.hpp
 * @details This header has all public API users may want to use. This mainly
 * consists of virtual classes:
 * - Prefetch;
 * - MemoryComponent;
 * - BranchPredictor;
 * - BranchTargetPredictor;
 * - ReorderBuffer;
 */

#include <cstdio>
#include <cstring>

namespace sinuca {

/** @brief Types of configuration values. */
enum ConfigValueType {
    ConfigValueTypeBoolean,
    ConfigValueTypeNumber,
    ConfigValueTypeInteger,
    ConfigValueTypeComponentReference,
};

// Pre-declaration for ConfigValue.
class Component;

/** @brief Tagged union of ConfigValueType. */
struct ConfigValue {
    ConfigValueType type;
    union {
        bool boolean;
        double number;
        long integer;
        Component* componentReference;
    } value;
};

/** @brief Everything is a component. */
class Component {
  public:
    /**
     * @brief Called when the SimulatorBuilder parses a config parameter inside
     * a component.
     * @param parameter The name (key) of the parameter.
     * @param value The value.
     * @return 0 when success, 1 if there's a problem with the configuration
     * parameter.
     */
    virtual int SetConfigParameter(const char* parameter,
                                   ConfigValue value) = 0;
    /** @brief Needed because of C++. */
    inline virtual ~Component() {}
};

#define COMPONENT(type) \
    if (!strcmp(name, #type)) return new type

#define COMPONENTS(components)                                 \
    namespace sinuca {                                         \
    Component* CreateCustomComponentByClass(const char* name) { \
        components;                                            \
        return 0;                                              \
    }                                                          \
    }

/** @brief Don't call, used by the SimulatorBuilder. */
Component* CreateDefaultComponentByClass(const char* name);
/** @brief Don't call, used by the SimulatorBuilder. */
Component* CreateCustomComponentByClass(const char* name);

// Pre-defines for MemoryPacket.
class MemoryRequester;
class MemoryComponent;

/** @brief Exchanged between MemoryComponent and MemoryRequester. */
struct MemoryPacket {
    MemoryRequester* respondTo;
    MemoryComponent* responser;
    /** @brief Components may use it to identify specific packets. */
    const long id;
};

/** @brief Exchanged between the engine, FirstStagePipeline and other pipeline
 * stages. */
struct InstructionPacket {};

/** @brief MemoryComponents can receive memory requests from MemoryRequesters.
 * */
class MemoryComponent {
  public:
    /** @brief Called by MemoryRequesters. */
    virtual void Request(MemoryPacket packet) = 0;
};

/** @brief MemoryRequesters send memory requests to MemoryComponents. */
class MemoryRequester {
  public:
    /** @brief Called by MemoryComponents in response to a specific packet. */
    virtual void Response(MemoryPacket packet) = 0;
};

class Prefetch {};

class BranchPredictor {};

class BranchTargetPredictor {};

class ReorderBuffer {};

}  // namespace sinuca

#define SINUCA3_HPP_
#endif  // SINUCA3_HPP_
