#ifndef PINTOOL_HPP_

#include <cstddef> // size_t
#include <cstring> // memcpy
#include "src/trace_reader/sinuca3_trace_reader.hpp"

// branchType are not written to struct
#define SIZE_DATA_INS sizeof(DataINS) \
                    - sizeof(sinuca::traceReader::sinuca3TraceReader::Branch)
#define BUFFER_SIZE 1<<20

inline void copy(char* buf, size_t* used, void* src, size_t size) {
    memcpy(buf+*used, src, size);
    (*used)+=size;
}

inline void setBit(unsigned char *byte, int position) {
    *byte |= (1 << position);
}

struct DataINS {
    long addr;
    unsigned int baseReg;
    unsigned int indexReg;
    unsigned char size;
    unsigned char booleanValues;
    unsigned char numReads;
    unsigned char numWrites;
    unsigned char branchType;
} __attribute__((packed)); // no padding

struct DataMEM {
    long addr;
    int size;
} __attribute__((packed)); // no padding

#define PINTOOL_HPP_
#endif