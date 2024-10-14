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
 * @file simulator_builder.cpp
 * @brief Implementation of the SimulatorBuilder class.
 */

#include "simulator_builder.hpp"

#include <yaml.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../utils/logging.hpp"

using namespace sinuca;

config::SimulatorBuilder::SimulatorBuilder()
    : components((ComponentWithName*)malloc(
          sizeof(*this->components) * config::DEFAULT_COMPONENT_ARRAY_SIZE)),
      capacity(config::DEFAULT_COMPONENT_ARRAY_SIZE),
      size(config::DEFAULT_COMPONENT_ARRAY_SIZE) {
    assert(this->components != NULL);
}

config::SimulatorBuilder::~SimulatorBuilder() { free(this->components); }

sinuca::config::ComponentWithName*
config::SimulatorBuilder::GetComponentReferenceByName(const char* name) {
    for (int i = 0; i < this->size; ++i) {
        if (!strcmp(name, this->components[i].name))
            return &this->components[i];
    }
    return NULL;
}

int config::SimulatorBuilder::ParseConfigParameter(const char* componentName,
                                                   const char* parameter,
                                                   ConfigValue* value) {
    if (sscanf(parameter, "%ld", &value->value.integer) == 1) {
        value->type = ConfigValueTypeInteger;
    } else if (sscanf(parameter, "%lf", &value->value.number) == 1) {
        value->type = ConfigValueTypeNumber;
    } else if (!strcmp(parameter, "true") || !strcmp(parameter, "yes")) {
        value->type = ConfigValueTypeBoolean;
        value->value.boolean = true;
    } else if (!strcmp(parameter, "false") || !strcmp(parameter, "no")) {
        value->type = ConfigValueTypeBoolean;
        value->value.boolean = false;
    } else {
        // So it must be an object reference because we don't support sending
        // strings. Let's try to find it.
        value->type = ConfigValueTypeComponentReference;
        config::ComponentWithName* comp =
            this->GetComponentReferenceByName(parameter);
        if (comp == NULL) {
            SINUCA3_ERROR_PRINTF(
                "On declaration of component %s: no such component %s",
                componentName != NULL ? componentName : "<inline>", parameter);
            return 1;
        }
        value->value.componentReference = comp->component;
    }

    return 0;
}

inline int TryToSetConfigParameter(config::ComponentWithName* component,
                                   const char* key, ConfigValue value) {
    // Yet another place to change when adding new parameter types. But it is
    // what it is...
    if (component->component->SetConfigParameter(key, value)) {
        SINUCA3_ERROR_PRINTF(
            "Component %s does not accept config parameter %s with type ",
            component->name != NULL ? component->name : "<inline>", key);
        switch (value.type) {
            case ConfigValueTypeInteger:
                SINUCA3_ERROR_PRINTF("integer\n");
                break;
            case ConfigValueTypeBoolean:
                SINUCA3_ERROR_PRINTF("boolean\n");
                break;
            case ConfigValueTypeNumber:
                SINUCA3_ERROR_PRINTF("number\n");
                break;
            case ConfigValueTypeComponentReference:
                SINUCA3_ERROR_PRINTF("object reference\n");
                break;
        }
        return 1;
    }
    return 0;
}

int config::SimulatorBuilder::ParseComponentParameter(
    yaml_parser_t* parser, const char* key, const yaml_event_t* value,
    ComponentWithName* component) {
    // Inline component.
    if (value->type == YAML_MAPPING_START_EVENT) {
        // Warn the user as we don't support aliasing to inline components.
        if (value->data.mapping_start.anchor != NULL)
            SINUCA3_WARNING_PRINTF(
                "Trying to create an anchor %s to an inline component, which "
                "is not supported. Ignoring anchor.\n",
                value->data.mapping_start.anchor);
        return this->InstantiateNewComponent(parser, NULL);
    }

    if (value->type != YAML_SCALAR_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "On declaration of component %s: parameter value is not an inline "
            "component nor a scalar.\n",
            component->name != NULL ? component->name : "<inline>");
        return 1;
    }

    // Everything is alright until now. Let's try to parse the value.
    const char* valueString = (const char*)value->data.scalar.value;
    ConfigValue configValue;
    if (ParseConfigParameter(component->name, valueString, &configValue))
        return 1;

    return TryToSetConfigParameter(component, key, configValue);
}

void config::SimulatorBuilder::AddComponentToArray(
    config::ComponentWithName component) {
    if (this->capacity == this->size) {
        this->components = (ComponentWithName*)reallocarray(
            this->components, this->capacity * 2, sizeof(*this->components));
        this->capacity *= 2;
    }
    this->components[this->size] = component;
    this->size += 1;
}

inline int EnsureComponentIsMapping(const char* name, yaml_parser_t* parser,
                                    yaml_event_t* event) {
    // Ensure component is a mapping.
    if (!yaml_parser_parse(parser, event)) {
        SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
        return 1;
    }
    if (event->type != YAML_MAPPING_START_EVENT) {
        SINUCA3_ERROR_PRINTF("Component definition %s is not a YAML mapping.\n",
                             name != NULL ? name : "<inline>");
        yaml_event_delete(event);
        return 1;
    }
    return 0;
}

inline void MakeComponentNameAndAnchor(config::ComponentWithName* newComponent,
                                       const char* name, yaml_event_t* event) {
    // Create the name.
    if (name != NULL) {
        long nameSize = strlen(name);
        newComponent->name = new char[nameSize + 1];
        memcpy(newComponent->name, name, nameSize + 1);
    }

    // Try to get an anchor.
    if (event->data.mapping_start.anchor != NULL) {
        const char* anchorString =
            (const char*)event->data.mapping_start.anchor;
        long anchorSize = strlen(anchorString);
        newComponent->anchor = new char[anchorSize + 1];
        memcpy(newComponent->anchor, anchorString, anchorSize + 1);
    }
}

inline int EnsureNextKeyIsComponentClass(const char* name,
                                         yaml_parser_t* parser) {
    yaml_event_t event;
    int ret = 0;

    if (!yaml_parser_parse(parser, &event)) {
        SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
        return 1;
    }

    const char* eventString = (const char*)event.data.scalar.value;
    if (strcmp(eventString, "component")) {
        SINUCA3_ERROR_PRINTF(
            "On declaration of component %s: First component parameter should "
            "be the component name (component: <class>).\n",
            name != NULL ? name : "<inline>");
        ret = 1;
    }

    yaml_event_delete(&event);
    return ret;
}

inline Component* GetComponentClassAndNewIt(const char* name,
                                            yaml_parser_t* parser) {
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event)) {
        SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
        return NULL;
    }

    const char* eventString = (const char*)event.data.scalar.value;
    Component* component = CreateDefaultComponentByClass(eventString);

    if (component == NULL) {
        component = CreateCustomComponentByClass(eventString);

        if (component == NULL) {
            SINUCA3_ERROR_PRINTF(
                "On declaration of component %s: No such component: %s\n",
                name != NULL ? name : "<inline>", eventString);
        }
    }

    yaml_event_delete(&event);
    return component;
}

inline int MakeComponentBoilerplate(const char* name,
                                    config::ComponentWithName* newComponent,
                                    yaml_parser_t* parser) {
    yaml_event_t mappingEvent;

    if (EnsureComponentIsMapping(name, parser, &mappingEvent)) return 1;
    if (EnsureNextKeyIsComponentClass(name, parser)) {
        yaml_event_delete(&mappingEvent);
        return 1;
    }

    newComponent->component = GetComponentClassAndNewIt(name, parser);
    if (newComponent->component == NULL) {
        yaml_event_delete(&mappingEvent);
        return 1;
    }

    MakeComponentNameAndAnchor(newComponent, name, &mappingEvent);
    yaml_event_delete(&mappingEvent);
    return 0;
}

int config::SimulatorBuilder::InstantiateNewComponent(yaml_parser_t* parser,
                                                      const char* name) {
    ComponentWithName newComponent = {NULL, NULL, NULL};

    if (MakeComponentBoilerplate(name, &newComponent, parser)) return 1;

    // Now parse component parameters. The break of this loop is in the middle,
    // between parsing event and valueEvent.
    yaml_event_t event;
    yaml_event_t valueEvent;
    for (;;) {
        if (!yaml_parser_parse(parser, &event)) {
            SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
            goto cleanup_error_component;
        }

        // Finished adding component!
        if (event.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&event);
            this->AddComponentToArray(newComponent);
            return 0;
        }

        if (event.type != YAML_SCALAR_EVENT) {
            SINUCA3_ERROR_PRINTF(
                "On declaration of component %s: expected a scalar value as "
                "key.\n",
                name != NULL ? name : "<inline>");
            goto cleanup_error_event;
        }

        if (!yaml_parser_parse(parser, &valueEvent)) {
            SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
            goto cleanup_error_event;
        }

        if (this->ParseComponentParameter(parser,
                                          (const char*)event.data.scalar.value,
                                          &valueEvent, &newComponent)) {
            SINUCA3_ERROR_PRINTF("When declaring component %s\n",
                                 name != NULL ? name : "<inline>");
            break;
        }

        yaml_event_delete(&event);
        yaml_event_delete(&valueEvent);
    }

    yaml_event_delete(&valueEvent);
cleanup_error_event:
    yaml_event_delete(&event);
cleanup_error_component:
    delete newComponent.component;
    if (newComponent.anchor != NULL) delete[] newComponent.anchor;
    if (newComponent.name != NULL) delete[] newComponent.name;

    return 1;
}

int config::SimulatorBuilder::IncludeFiles(yaml_parser_t* parser) {
    yaml_event_t event;
    int ret = 1;

    if (!yaml_parser_parse(parser, &event)) {
        SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
        return 1;
    }

    // When just a single file.
    if (event.type == YAML_SCALAR_EVENT) {
        const char* fileToInclude = (const char*)event.data.scalar.value;
        ret = this->ParseFile(fileToInclude);
        if (ret != 0)
            SINUCA3_ERROR_PRINTF("While including file %s\n", fileToInclude);
        goto cleanup;
    }

    if (event.type != YAML_SEQUENCE_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "Include parameter should be string or list of strings.\n");
        goto cleanup;
    }

    // When multiple files.
    for (;;) {
        yaml_event_delete(&event);

        if (!yaml_parser_parse(parser, &event)) {
            SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
            return 1;
        }

        if (event.type == YAML_SEQUENCE_END_EVENT) {
            ret = 0;
            break;
        }

        if (event.type != YAML_SCALAR_EVENT) {
            SINUCA3_ERROR_PRINTF(
                "Include parameter in a list should be a string.\n");
            goto cleanup;
        }

        const char* fileToInclude = (const char*)event.data.scalar.value;
        ret = this->ParseFile(fileToInclude);
        if (ret != 0) {
            SINUCA3_ERROR_PRINTF("While including file %s\n", fileToInclude);
            goto cleanup;
        }
    }
cleanup:
    yaml_event_delete(&event);

    return ret;
}

inline int SkipYAMLBoilerplateEvents(const char* filePath,
                                     yaml_parser_t* parser) {
    yaml_event_t event;
    int ret = 0;

    // Skip stream start event.
    if (!yaml_parser_parse(parser, &event)) {
        SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
        return 1;
    }
    yaml_event_delete(&event);

    // Skip document start event.
    if (!yaml_parser_parse(parser, &event)) {
        SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
        return 1;
    }
    yaml_event_delete(&event);

    // Ensure the file is a mapping.
    if (!yaml_parser_parse(parser, &event)) {
        SINUCA3_ERROR_PRINTF("%s: %s\n", parser->context, parser->problem);
        return 1;
    }
    if (event.type != YAML_MAPPING_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "Error parsing config file %s: file toplevel is not a YAML "
            "mapping.\n",
            filePath);
        ret = 1;
    }
    yaml_event_delete(&event);
    return ret;
}

int config::SimulatorBuilder::ParseFile(const char* filePath) {
    FILE* fp = fopen(filePath, "r");
    if (fp == NULL) {
        SINUCA3_ERROR_PRINTF("No such configuration file: %s\n", filePath);
        return 1;
    }

    int ret = 1;

    yaml_parser_t parser;
    memset(&parser, 0, sizeof(parser));
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fp);

    if (SkipYAMLBoilerplateEvents(filePath, &parser)) goto cleanup;

    yaml_event_t event;

    // Here in the file toplevel we expect the following things:
    // - include;
    // - new component (i.e., a mapping);
    for (;;) {
        yaml_event_delete(&event);

        if (!yaml_parser_parse(&parser, &event)) {
            SINUCA3_ERROR_PRINTF("%s: %s\n", parser.context, parser.problem);
            goto cleanup;
        }

        const char* string;  // For scalar values.
        switch (event.type) {
            case YAML_STREAM_END_EVENT:
                goto end_loop;
            // New toplevel parameters shall be added here.
            case YAML_SCALAR_EVENT:
                string = (const char*)event.data.scalar.value;
                if (!strcmp(string, "include")) {
                    if (this->IncludeFiles(&parser)) goto cleanup_del_event;
                } else {
                    if (this->InstantiateNewComponent(&parser, string))
                        goto cleanup_del_event;
                }
                break;
            default:
                SINUCA3_ERROR_PRINTF(
                    "Expected a key on config file toplevel.\n");
                goto cleanup_del_event;
        }
    }
end_loop:
    ret = 0;

cleanup_del_event:
    yaml_event_delete(&event);
cleanup:
    yaml_parser_delete(&parser);
    fclose(fp);
    return ret;
}

Engine* config::SimulatorBuilder::InstantiateSimulationEngine(
    const char* configFile) {
    if (this->ParseFile(configFile)) return NULL;
    Engine* engine = new Engine();
    return engine;
}
