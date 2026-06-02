NDEBUG = -DNDEBUG # Set NDEBUG="" via command line to enable debug mode (e.g., make NDEBUG="")
SINUCA_TRACER_DIR = ../src/tracer/sinuca/
SINUCA_LOGGER = ../src/utils/logger

TOOL_ROOTS = sinuca3_pintool
HANDLER = file_handler
INTRINSICS = intrinsics
LOGGER = logger

OBJ_DEPS = $(OBJDIR)$(TOOL_ROOTS)$(OBJ_SUFFIX) \
		$(OBJDIR)$(HANDLER)$(OBJ_SUFFIX) \
		$(OBJDIR)$(INTRINSICS)$(OBJ_SUFFIX) \
		$(OBJDIR)$(LOGGER)$(OBJ_SUFFIX) \

# This section contains the build rules for all binaries that have special build rules.
# See makefile.default.rules for the default build rules.

$(OBJDIR)$(TOOL_ROOTS)$(OBJ_SUFFIX): $(TOOL_ROOTS).cpp
	$(CXX) $(TOOL_CXXFLAGS) -I../src -I. $(COMP_OBJ)$@ $< $(NDEBUG)

$(OBJDIR)$(HANDLER)$(OBJ_SUFFIX): $(SINUCA_TRACER_DIR)$(HANDLER).cpp
	$(CXX) $(TOOL_CXXFLAGS) -I../src -I. $(COMP_OBJ)$@ $< $(NDEBUG)

$(OBJDIR)$(LOGGER)$(OBJ_SUFFIX): $(SINUCA_LOGGER).cpp
	$(CXX) $(TOOL_CXXFLAGS) -I../src -I. $(COMP_OBJ)$@ $< $(NDEBUG)

$(OBJDIR)$(INTRINSICS)$(OBJ_SUFFIX): $(INTRINSICS).cpp
	$(CXX) $(TOOL_CXXFLAGS) -I../src -I. $(COMP_OBJ)$@ $< $(NDEBUG)

$(OBJDIR)$(TOOL_ROOTS)$(PINTOOL_SUFFIX): $(OBJ_DEPS)
	$(LINKER) $(TOOL_LDFLAGS) $(LINK_EXE)$@ $^ $(TOOL_LPATHS) $(TOOL_LIBS)
