# --------------------------------------------------
# hdl_sim build
#
#   make                  build ./simulator (optimized, same as make release)
#   make debug            build ./simulator-debug (-O0)
#   make asan             build ./simulator-asan (AddressSanitizer + UBSan)
#   make test             build and run every test (debug build unless MODE=...)
#   make test TESTS=dff   run tests whose name contains "dff" (globs work too)
#   make test ARGS=-v     pass options to the runner (see: make test ARGS=--help)
#   make test-asan        same tests under AddressSanitizer + UBSan + checked STL
#   make test-list        list the available tests
#   make test-old         build and run the archived pre-refactor tests (tests/archive)
#   make test-old TESTS=and   only archived tests whose path contains "and"
#   make clean
#
# The library is compiled once into build/<mode>/libhdlsim.a and every test
# links against it; header dependencies are tracked, so after an edit only the
# files that include it are rebuilt.
# --------------------------------------------------

CXX      ?= g++
MODE     ?= release
BUILD    := build/$(MODE)
NPROC    := $(shell nproc 2>/dev/null || echo 4)

CPPFLAGS := -I./include -MMD -MP
CXXFLAGS := -std=c++17 -Wall -Wextra -g
LDFLAGS  :=

ifeq ($(MODE),debug)
  CXXFLAGS += -O0
else ifeq ($(MODE),asan)
  # _GLIBCXX_DEBUG turns invalid iterator use into a hard error
  CXXFLAGS += -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
  CPPFLAGS += -D_GLIBCXX_DEBUG
  LDFLAGS  += -fsanitize=address,undefined
else ifeq ($(MODE),release)
  CXXFLAGS += -O2
else
  $(error unknown MODE '$(MODE)' (use debug, asan or release))
endif

# One binary per mode, so switching modes never reuses a stale link
ifeq ($(MODE),release)
  TARGET = simulator
else
  TARGET = simulator-$(MODE)
endif

APP_SRC = main.cpp

LIB_SRC = \
      $(wildcard src/core/*.cpp) \
      $(wildcard src/combinational/*.cpp) \
      $(wildcard src/sequential/*.cpp) \
      $(wildcard src/primary/*.cpp) \
      $(wildcard src/operations/*.cpp) \
      $(wildcard src/simulation/*.cpp) \
      $(wildcard src/parser/*.cpp) \
      $(wildcard src/synthesizer/*.cpp)

LIB_OBJ = $(patsubst %.cpp,$(BUILD)/%.o,$(LIB_SRC))
LIB     = $(BUILD)/libhdlsim.a
APP_OBJ = $(patsubst %.cpp,$(BUILD)/%.o,$(APP_SRC))


# --------------------------------------------------
# Main simulator
# --------------------------------------------------

all: $(TARGET)

release debug asan:
	@$(MAKE) --no-print-directory -j$(NPROC) MODE=$@

$(TARGET): $(APP_OBJ) $(LIB)
	$(CXX) $(LDFLAGS) $^ -o $@

$(LIB): $(LIB_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


# --------------------------------------------------
# Tests: everything under tests/ (except tests/archive) links into one runner
# --------------------------------------------------

TEST_SRC = $(shell find tests -path tests/archive -prune -o -type f -name '*.cpp' -print)
TEST_OBJ = $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRC))
TEST_BIN = $(BUILD)/run_tests

# Flip-flops whose header is still empty are not compiled into the tests yet;
# their tests switch on automatically (HDLSIM_HAVE_<NAME>) once implemented.
PENDING_FFS = DFF_N TFF_P TFF_N JKFF_P JKFF_N
HAVE_FLAGS := $(foreach ff,$(PENDING_FFS),$(if $(shell test -s include/sequential/$(ff).hpp && echo y),-DHDLSIM_HAVE_$(ff)))

$(TEST_OBJ): CPPFLAGS += -I./tests -DHDLTEST_FIXTURE_DIR='"$(CURDIR)/tests/fixtures"' $(HAVE_FLAGS)

$(TEST_BIN): $(TEST_OBJ) $(LIB)
	$(CXX) $(LDFLAGS) $^ -o $@

TESTS ?=
ARGS  ?=

# Tests build in debug mode unless a mode is given on the command line
TEST_MODE := $(if $(filter command line,$(origin MODE)),$(MODE),debug)
TEST_RUNNER = build/$(TEST_MODE)/run_tests

test:
	@$(MAKE) --no-print-directory -j$(NPROC) MODE=$(TEST_MODE) $(TEST_RUNNER)
	@$(TEST_RUNNER) $(ARGS) $(TESTS)

test-asan:
	@$(MAKE) --no-print-directory MODE=asan test

test-list:
	@$(MAKE) --no-print-directory -j$(NPROC) MODE=$(TEST_MODE) $(TEST_RUNNER)
	@$(TEST_RUNNER) --list $(TESTS)

check: test


# --------------------------------------------------
# Archived tests: the old standalone programs, one binary each
# --------------------------------------------------

OLD_TEST_SRC = $(shell find tests/archive -type f -name '*.cpp')
OLD_TEST_BIN = $(patsubst %.cpp,build/$(TEST_MODE)/%,$(OLD_TEST_SRC))

$(BUILD)/tests/archive/%: tests/archive/%.cpp $(LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $< $(LIB) -o $@

test-old:
	@$(MAKE) --no-print-directory -j$(NPROC) MODE=$(TEST_MODE) $(OLD_TEST_BIN)
	@tests/archive/run.sh build/$(TEST_MODE)/tests/archive $(TESTS)


# --------------------------------------------------
# Run
# --------------------------------------------------

run: $(TARGET)
	./$(TARGET)


# --------------------------------------------------
# Clean
# --------------------------------------------------

clean:
	rm -rf build
	rm -f simulator simulator-debug simulator-asan


-include $(LIB_OBJ:.o=.d) $(APP_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

.PHONY: all release debug asan test test-asan test-list test-old check run clean
