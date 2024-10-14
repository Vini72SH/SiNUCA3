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
 * @file engine.cpp
 * @brief Implementation of the simulation engine.
 */

#include "engine.hpp"
#include "../utils/logging.hpp"

using namespace sinuca;

int Engine::AddFirstStage(Component *component) {
    this->firstStage = dynamic_cast<FirstStagePipeline *>(component);
    return (this->firstStage == NULL);
}

int Engine::Simulate() {
    SINUCA3_DEBUG_PRINTF("Simulation starting.\n");
    SINUCA3_DEBUG_PRINTF("Simulation ending.\n");

    return 0;
}
