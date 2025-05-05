#include "file_handler.hpp"

#include "../utils/logging.hpp"

trace::TraceFile::TraceFile(const char *prefix, const char *imageName,
                            const char *sufix, std::string tracePath) {
    this->buf = new unsigned char[BUFFER_SIZE];
    this->offset = 0;
    this->filePath = tracePath + prefix + imageName + sufix + ".trace";
}

trace::TraceFile::~TraceFile() {
    delete[] this->buf;
    fclose(this->file);
}

trace::TraceFileReader::TraceFileReader(const char *prefix,
                                        const char *imageName,
                                        const char *sufix,
                                        const char *traceFolderPath)
    : TraceFile(prefix, imageName, sufix, std::string(traceFolderPath)) {
    this->file = fopen(this->filePath.c_str(), "rb");
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open => %s\n", this->filePath.c_str());
    }
}

int trace::TraceFileReader::ReadBuffer() {
    size_t result = fread(this->buf, 1, this->bufSize, file);
    this->offset = 0;

    if (result <= 0) {
        return 1;
    } else if (result < this->bufSize) {
        this->eofLocation = result;
    }

    return 0;
}

int trace::TraceFileReader::ReadBufSizeFromFile() {
    size_t read = fread(&this->bufSize, 1, sizeof(this->bufSize), this->file);
    if (read <= 0) {
        return 1;
    }
    return 0;
}

trace::TraceFileGenerator::TraceFileGenerator(const char *prefix,
                                              const char *imageName,
                                              const char *sufix,
                                              const char *traceFolderPath)
    : TraceFile(prefix, imageName, sufix, std::string(traceFolderPath)) {
    this->file = fopen(this->filePath.c_str(), "wb");
}

void trace::TraceFileGenerator::WriteToBuffer(void *src, size_t size) {
    if (this->offset + size >= BUFFER_SIZE) this->FlushBuffer();

    memcpy(this->buf + this->offset, src, size);
    this->offset += size;
}

void trace::TraceFileGenerator::FlushBuffer() {
    size_t written = fwrite(this->buf, 1, this->offset, this->file);
    assert(written == this->offset && "fwrite returned something wrong");
    this->offset = 0;
}