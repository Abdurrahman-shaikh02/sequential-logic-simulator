CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./include -g

TARGET = simulator

APP_SRC = main.cpp

LIB_SRC = \
      $(wildcard src/core/*.cpp) \
      $(wildcard src/combinational/*.cpp) \
      $(wildcard src/sequential/*.cpp) \
      $(wildcard src/primary/*.cpp) \
      $(wildcard src/operations/*.cpp) \
      $(wildcard src/simulation/*.cpp) \
      $(wildcard src/parser/*.cpp) \
      $(wildcard src/synthesizer/*.cpp) \
      $(wildcard src/testbench/*.cpp) \
      $(wildcard src/assertion/*.cpp) \
      $(wildcard src/waveform/*.cpp)

# Category implementations are organized under tests/<category>/.
TEST_CATEGORY_SRC = $(wildcard tests/unit/Test*.cpp) \
      $(wildcard tests/integration/Test*.cpp) \
      $(wildcard tests/validation/Test*.cpp) \
      $(wildcard tests/blackbox/Test*.cpp) \
      $(wildcard tests/whitebox/Test*.cpp) \
      $(wildcard tests/regression/Test*.cpp) \
      $(wildcard tests/system/Test*.cpp)

SRC = $(APP_SRC) $(LIB_SRC) $(TEST_CATEGORY_SRC)

OBJ = $(patsubst %.cpp,build/%.o,$(SRC))
LIB_OBJ = $(patsubst %.cpp,build/%.o,$(LIB_SRC))
TEST_CATEGORY_OBJ = $(patsubst %.cpp,build/%.o,$(TEST_CATEGORY_SRC))


# --------------------------------------------------
# Main simulator
# --------------------------------------------------

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@


build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


# --------------------------------------------------
# Tests
# --------------------------------------------------

TEST_SUITE_BIN = tests/test_suite

PARSER_TEST_BIN = build/standalone/tests/unit/parser/test_parser

test: $(TEST_SUITE_BIN) $(PARSER_TEST_BIN)
	./$(TEST_SUITE_BIN) all
	./$(PARSER_TEST_BIN)

$(TEST_SUITE_BIN): tests/test_suite.cpp $(LIB_OBJ) $(TEST_CATEGORY_OBJ)
	@mkdir -p build/tests
	$(CXX) $(CXXFLAGS) tests/test_suite.cpp $(LIB_OBJ) $(TEST_CATEGORY_OBJ) -o $@


# --------------------------------------------------
# Run & GUI
# --------------------------------------------------

run: $(TARGET)
	./$(TARGET)

gui: $(TARGET)
	python3 gui/server.py 8080


# --------------------------------------------------
# Clean
# --------------------------------------------------

clean:
	rm -rf build output
	rm -f $(TARGET)
	rm -f $(TEST_SUITE_BIN)


.PHONY: test run clean gui

# Standalone C++ tests have their own main() functions and must be linked separately.
STANDALONE_SRC = $(filter-out tests/unit/TestUnit.cpp,$(shell find tests/unit tests/netlists -type f -name '*.cpp'))
STANDALONE_BINS = $(patsubst %.cpp,build/standalone/%,$(STANDALONE_SRC))

build/standalone/%: %.cpp $(LIB_OBJ) $(TEST_CATEGORY_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $< $(LIB_OBJ) $(TEST_CATEGORY_OBJ) -o $@

standalone-build: $(STANDALONE_BINS)
	@echo "Standalone executables built under build/standalone/"

# Tests requiring stdin, hardcoded working directories or external resources may
# need manual inputs; do not treat successful compilation as a passed test.
.PHONY: standalone-build

# End-to-end testbench examples (including the generic vending-machine FSM).
examples-test: $(TARGET)
	./$(TARGET) --testbench examples/and_gate/and_gate.tb
	./$(TARGET) --testbench examples/traffic_light/traffic_light.tb
	./$(TARGET) --testbench examples/vending_machine/vending_machine.tb

.PHONY: examples-test


# Focused parser regression suite (includes supported sequential primitive types).
parser-test: $(PARSER_TEST_BIN)
	./$(PARSER_TEST_BIN)

.PHONY: parser-test
