#ifndef SINUCA3_GENERATOR_DYNAMIC_FILE_HPP_
#define SINUCA3_GENERATOR_DYNAMIC_FILE_HPP_

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
 * @file dynamic_trace_writer.hpp
 * @details .
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>
#include "utils/logging.hpp"

namespace sinucaTracer {

class DynamicTraceWriter {
  private:
    FILE *file;
    FileHeader header;
    DynamicTraceRecord record;

    inline void InitFileHeader() {
        /* reserve space for header */
        fseek(this->file, sizeof(this->header), SEEK_SET);
        this->header.fileType = FileTypeDynamicTrace;
        this->header.data.dynamicHeader.totalExecutedInstructions = 0;
    }
    inline int WriteHeaderToFile() {
        rewind(this->file);
        return (fwrite(&this->header, sizeof(this->header), 1, this->file) !=
                sizeof(this->header));
    }

  public:
    inline DynamicTraceWriter() : file(NULL) { this->InitFileHeader(); };
    inline ~DynamicTraceWriter() {
        if (file) {
            fclose(file);
        }
        if (this->WriteHeaderToFile()) {
            SINUCA3_ERROR_PRINTF("Failed to write dynamic file header!\n");
        }
    }

    int OpenFile(const char *source, const char *img, int tid);
    int AddBasicBlockId(int id);
    int AddThreadEvent(unsigned char event, int tid);

    inline void IncTotalExecInst(int ins) {
        this->header.data.dynamicHeader.totalExecutedInstructions += ins;
    }
};

}  // namespace sinucaTracer

#endif
