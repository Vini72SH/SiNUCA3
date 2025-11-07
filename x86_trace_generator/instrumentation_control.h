#ifndef SINUCA3_INSTRUMENTATION_CONTROL_H_
#define SINUCA3_INSTRUMENTATION_CONTROL_H_

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
 * @file instrumentation_control.h
 * @brief API for instrumenting applications with SiNUCA3.
 */

/**
 * @brief Begins an instrumentation block.
 *
 * Any code appearing after this call (until the exectuion of a
 * corresponding EndInstrumentationBlock call) becomes will be instrumented,
 * meaning that analysis code may be inserted into target program
 * during the instrumentation phase.
 */
__attribute__((noinline)) void BeginInstrumentationBlock() {
    __asm__ volatile("" :::);
}

/**
 * @brief Ends an instrumentation block.
 *
 * Code following this call will no longer be instrumented.
 * This must be paired with a preceding BeginInstrumentationBlock call.
 */
__attribute__((noinline)) void EndInstrumentationBlock() {
    __asm__ volatile("" :::);
}

#endif
