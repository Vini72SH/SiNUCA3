#ifndef READERFILEHANDLER_HPP_
#define READERFILEHANDLER_HPP_

#include "../utils/file_handler.hpp"

using namespace trace;

namespace traceReader {

  class StaticTraceFile : public TraceFileReader {

  };

  class DynamicTraceFile : public TraceFileReader {

  };

  class MemoryTraceFile : public TraceFileReader {

  };

} // namespace traceReader

#endif