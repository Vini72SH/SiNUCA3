#include "simulator_builder.hpp"

#include <cassert>
#include <cstring>
#include <vector>

#include "../utils/logging.hpp"
#include "yaml_parser.hpp"

// Pre-declaration for IncludeString as they're mutually recursive.
int ProcessIncludeEntries(sinuca::yaml::YamlValue* config,
                          const char* configFile);

int IncludeString(std::vector<sinuca::yaml::YamlMappingEntry*>* config,
                  const char* string) {
    sinuca::yaml::YamlValue* newFileValue = sinuca::yaml::ParseFile(string);
    if (newFileValue == NULL) return 1;

    assert(newFileValue->type == sinuca::yaml::YamlValueTypeMapping);
    std::vector<sinuca::yaml::YamlMappingEntry*>* newValues =
        newFileValue->value.mapping;

    if (ProcessIncludeEntries(newFileValue, string)) {
        delete newFileValue;
        return 1;
    }

    config->reserve(config->size() + newValues->size());
    for (unsigned int i = 0; i < newValues->size(); ++i) {
        config->push_back((*newValues)[i]);
    }

    // We should call clear before destroying the value so it does not destroy
    // the values we copied to the other vector.
    newValues->clear();
    delete newFileValue;

    return 0;
}

int IncludeArray(std::vector<sinuca::yaml::YamlMappingEntry*>* config,
                 std::vector<sinuca::yaml::YamlValue*>* array,
                 const char* configFile) {
    for (unsigned int i = 0; i < array->size(); ++i) {
        if ((*array)[i]->type != sinuca::yaml::YamlValueTypeString) {
            SINUCA3_ERROR_PRINTF(
                "while reading configuration file %s: include array members "
                "should all be string.",
                configFile);
            return 1;
        }

        if (IncludeString(config, (*array)[i]->value.string)) return 1;
    }

    return 0;
}

int ProcessIncludeEntries(sinuca::yaml::YamlValue* config,
                          const char* configFile) {
    assert(config->type == sinuca::yaml::YamlValueTypeMapping);

    std::vector<sinuca::yaml::YamlMappingEntry*>* configMapping =
        config->value.mapping;

    // We got two iterators because we must iterate every original entry, but we
    // remove include entries while iterating.
    unsigned int origSize = configMapping->size();
    unsigned int index = 0;
    for (unsigned int i = 0; i < origSize; ++i) {
        if (strcmp((*configMapping)[index]->name, "include") == 0) {
            sinuca::yaml::YamlMappingEntry* entry = (*configMapping)[index];
            switch (entry->value->type) {
                case sinuca::yaml::YamlValueTypeString:
                    if (IncludeString(configMapping,
                                      entry->value->value.string)) {
                        SINUCA3_ERROR_PRINTF(
                            "while reading configuration file %s.\n",
                            configFile);
                        return 1;
                    }
                    break;
                case sinuca::yaml::YamlValueTypeArray:
                    if (IncludeArray(configMapping, entry->value->value.array,
                                     configFile)) {
                        SINUCA3_ERROR_PRINTF(
                            "while reading configuration file %s.\n",
                            configFile);
                        return 1;
                    }
                    break;
                default:
                    SINUCA3_ERROR_PRINTF(
                        "while reading configuration file %s: include should "
                        "be a string or an array of strings.\n",
                        configFile);
                    return 1;
            }

            configMapping->erase(configMapping->begin() + index);
        } else {
            ++index;
        }
    }

    return 0;
}

sinuca::engine::Engine* sinuca::config::SimulatorBuilder::Instantiate(
    const char* configFile) {
    sinuca::yaml::YamlValue* config = sinuca::yaml::ParseFile(configFile);
    if (config == NULL) return NULL;

    if (ProcessIncludeEntries(config, configFile)) {
        delete config;
        return NULL;
    }

    sinuca::yaml::PrintYaml(config);

    engine::Engine* engine = new engine::Engine;

    return engine;
}
