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
 * @file main.cpp
 * @details Entry point. User interaction should go here. Besides, should mostly
 * just consume other public APIs.
 */

#include <getopt.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sinuca3.hpp>
#include <tracer/sinuca/trace_reader.hpp>
#include <tracer/trace_reader.hpp>
#include <utils/map.hpp>
#include <vector>
#include <yaml/yaml_parser.hpp>

#include "utils/logger.hpp"

// Include our testing facilities in debug mode.
#ifndef NDEBUG
#include <tests.hpp>
#endif

/**
 * @brief Prints licensing information.
 */
void license() {
    printf(
        "SiNUCA 3 - Simulator of Non-Uniform Cache Architectures, Third "
        "iteration.\n"
        "\n"
        " Copyright (C) 2026  HiPES - Universidade Federal do Paraná\n"
        "\n"
        " This program is free software: you can redistribute it and/or "
        "modify\n"
        " it under the terms of the GNU General Public License as published "
        "by\n"
        " the Free Software Foundation, either version 3 of the License, or\n"
        " (at your option) any later version.\n"
        "\n"
        " This program is distributed in the hope that it will be useful,\n"
        " but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        " GNU General Public License for more details.\n"
        "\n"
        " You should have received a copy of the GNU General Public License\n"
        " along with this program.  If not, see "
        "<https://www.gnu.org/licenses/>.\n"
        "\n");
}

/**
 * @brief Prints the usage of the program.
 */
void usage() {
    license();
    printf("\nUsage: sinuca3 [OPTIONS]\n\n");
    printf("Required options:\n");
    printf("  -c <file>     Configuration file for simulation\n");
    printf("  -t <file>     Path to folder containing trace file for "
        "simulation (can be specified multiple times)\n");
    printf("Other options:\n");
    printf("  -h            Show this help message\n");
    printf("  -l            Show license information\n");
    printf("  -T <reader>   Trace reader to use (default: 'sinuca3')\n");
    printf("  -L <0-3>      Set log level (0=Error, 1=Warning, 2=Info, 3=Debug)\n");
    printf("  -f <file>     Add logger file filter\n");
    printf("  -r <test>     Run a test (debug mode only)\n");
    printf("\nExample:\n");
    printf("  sinuca3 -c config.yaml -t path/to/trace1 -t path/to/trace2\n");
}

/**
 * @brief Returns a TraceReader from it's name. TODO: add default trace reader.
 */
TraceReader* AllocTraceReader(const char* traceReader) {
    if (strcmp(traceReader, "sinuca3") == 0)
        return new SinucaTraceReader;
    else
        return NULL;
}

/**
 * @brief Entry point.
 * @returns Non-zero on error.
 */
int main(int argc, char* const argv[]) {
    const char* traceReaderName = "sinuca3";
    const char* rootConfigFile = NULL;
    std::vector<TraceReader*> readers;
    std::vector<const char*> traces;

    char nextOpt;

    // When compiling debug mode, enable our testing facilities and set the log
    // level to debug.
#ifdef NDEBUG
    logger::Level logLevel = logger::LevelInfo;
#define SINUCA3_SWITCHES "lc:t:d:T:L:f:"
#else
    logger::Level logLevel = logger::LevelDebug;
#define SINUCA3_SWITCHES "r:lc:t:d:T:L:f:"
    const char* testToRun = NULL;
#endif

    while ((nextOpt = getopt(argc, argv, SINUCA3_SWITCHES)) != -1) {
        switch (nextOpt) {
            // When compiling in debug mode, enable our testing facilities.
#ifndef NDEBUG
            case 'r':
                testToRun = optarg;
                break;
#endif
            case 'c':
                rootConfigFile = optarg;
                break;
            case 't':
                traces.push_back(optarg);
                break;
            case 'T':
                traceReaderName = optarg;
                break;
            case 'l':
                license();
                return 0;
            case 'h':
                usage();
                return 0;
            case 'L':
                if (sscanf(optarg, "%d", &logLevel) != 1) {
                    SINUCA3_ERROR_PRINTF(
                        "Argument passed to -L is not an integer.\n");
                    return 1;
                }
                break;
            case 'f':
                SINUCA3_ADD_LOG_FILE_FILTER(optarg);
                break;
        }
    }

    if (SINUCA3_SET_LOG_LEVEL(logLevel) != 0) {
        SINUCA3_ERROR_PRINTF("Log level too big.\n");
        return 1;
    }

    // When compiling debug mode and there's a test to run, run it.
#ifndef NDEBUG
    if (testToRun != NULL) {
        int ret = Test(testToRun);
        if (ret < 0) {
            SINUCA3_LOG_PRINTF("No such test: %s\n", testToRun);
        } else if (ret > 0) {
            SINUCA3_LOG_PRINTF("Test failed with code %d.\n", ret);
        } else {
            SINUCA3_LOG_PRINTF("Test %s succeeded!\n", testToRun);
        }
        return ret;
    }
#endif

    if (rootConfigFile == NULL) {
        usage();
        return 1;
    }
    if (traces.empty()) {
        usage();
        return 1;
    }

    yaml::Parser parser;
    yaml::YamlValue configYamlValue;
    parser.ParseFileWithIncludes(rootConfigFile, &configYamlValue);

    assert(configYamlValue.type == yaml::YamlValueTypeMapping);

    std::vector<Linkable*> components;
    Engine engine;
    Map<Linkable*> aliases;
    Map<Definition> definitions;
    Config config =
        Config(&components, &aliases, &definitions,
               configYamlValue.value.mapping, configYamlValue.location);
    if (engine.Configure(config)) return 1;

    for (unsigned int i = 0; i < traces.size(); i++) {
        TraceReader* traceReader = AllocTraceReader(traceReaderName);
        if (traceReader == NULL) {
            SINUCA3_ERROR_PRINTF("The trace reader %s does not exist.",
                                 traceReaderName);
            return 1;
        }
        if (traceReader->OpenTrace(traces[i])) return 1;
        readers.push_back(traceReader);
    }

    engine.Simulate(&readers);
    for (unsigned int i = 0; i < readers.size(); i++) {
        readers[i]->PrintStatistics();
        delete readers[i];
    }

    return 0;
}