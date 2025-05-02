#ifndef FILEHANDLER_HPP_
#define FILEHANDLER_HPP_

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

const unsigned long BUFFER_SIZE = 1 << 20;
// Used in alignas to avoid false sharing
const unsigned long CACHE_LINE_SIZE = 64;
const unsigned long MAX_IMAGE_NAME_SIZE = 64;

namespace trace {

typedef unsigned int THREADID;
typedef unsigned int UINT32;

class TraceFile {
  protected:
    unsigned char *buf;
    FILE *file;
    size_t offset;  // in bytes
    std::string filePath;

    TraceFile(const char *prefix, const char *imageName, const char *sufix, std::string traceFolderPath) {
        this->buf = new unsigned char[BUFFER_SIZE];
        this->offset = 0;
        this->filePath = traceFolderPath + prefix +
                               imageName + sufix + ".trace";
    }

    virtual ~TraceFile() {
        delete[] this->buf;
        fclose(this->file);
    }
};

class TraceFileReader : public TraceFile {
  protected:
    TraceFileReader(const char *prefix, const char *imageName, const char *sufix,
                const char *traceFolderPath)
        : TraceFile(prefix, imageName, sufix, std::string(traceFolderPath)) {
        this->file = fopen(this->filePath.c_str(), "rb");
    }
};

class TraceFileGenerator : public TraceFile {
  protected:
    TraceFileGenerator(const char *prefix, const char *imageName, const char *sufix,
                const char *traceFolderPath)
        : TraceFile(prefix, imageName, sufix, std::string(traceFolderPath)) {
        this->file = fopen(this->filePath.c_str(), "wb");
    }

    void WriteToBuffer(void *src, size_t size) {
        if (this->offset + size >= BUFFER_SIZE) this->FlushBuffer();

        memcpy(this->buf + this->offset, src, size);
        this->offset += size;
    }

    virtual void FlushBuffer() {
        size_t written = fwrite(this->buf, 1, this->offset, this->file);
        assert(written == this->offset && "fwrite returned something wrong");
        this->offset = 0;
    }
};

}  // namespace trace

#endif
