#ifndef SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_
#define SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_

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
 * @file simulator_builder.hpp
 * @details Public API for reading a config file and instantiating a simulation
 * engine.
 */

#include <yaml.h>

#include <cstdio>

#include "../engine/engine.hpp"

namespace sinuca {
namespace config {

/**
 * @brief Used by the builder to instantiate and resolve component references.
 */
struct ComponentWithName {
    char* name;           /** @brief Self-explanatory. */
    char* anchor;         /** @brief Used to resolve copies with alias. */
    Component* component; /** @brief Self-explanatory. */
};

const long DEFAULT_COMPONENT_ARRAY_SIZE = 4096 / sizeof(ComponentWithName);

/**
 * @brief Reads a YAML configuration file and instantiates the system.
 */
class SimulatorBuilder {
  private:
    ComponentWithName* components; /** @brief Components array. */
    long capacity;                 /** @brief Capacity of components. */
    long size; /** @brief Amount of instantiated components. */

    // Internal methods.
    int ParseConfigParameter(const char* componentName, const char* parameter,
                             ConfigValue* value);
    void AddComponentToArray(ComponentWithName component);
    ComponentWithName* GetComponentReferenceByName(const char* name);
    int ParseFile(const char* filePath);
    int IncludeFiles(yaml_parser_t* parser);
    int InstantiateNewComponent(yaml_parser_t* parser, const char* string);
    int ParseComponentParameter(yaml_parser_t* parser, const char* key,
                                const yaml_event_t* value,
                                ComponentWithName* component);

  public:
    SimulatorBuilder();
    ~SimulatorBuilder();

    /** @brief This is the main method to use.+/ */
    Engine* InstantiateSimulationEngine(const char* configFile);
};

}  // namespace config
}  // namespace sinuca

#endif  // SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_
