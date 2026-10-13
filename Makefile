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
      $(wildcard src/synthesizer/*.cpp)

SRC = $(APP_SRC) $(LIB_SRC)

OBJ = $(patsubst %.cpp,build/%.o,$(SRC))


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

TEST_SRC = $(shell find tests -type f -name '*.cpp')
TEST_BIN = $(TEST_SRC:.cpp=)

test: $(TEST_BIN)

tests/%: tests/%.cpp $(LIB_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@


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
	rm -f $(TARGET)
	rm -f $(TEST_BIN)


.PHONY: test run clean


# --------------------------------------------------
# Worklet 3: Automated tests
# --------------------------------------------------

WORKLET3_GATE_TEST = tests/worklet3/gate_assertions.exe
WORKLET3_QUEUE_TEST = tests/worklet3/operation_queue_tests.exe

$(WORKLET3_GATE_TEST): tests/worklet3/gate_assertions.cpp $(LIB_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(WORKLET3_QUEUE_TEST): tests/worklet3/operation_queue_tests.cpp $(LIB_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

test-worklet3: $(WORKLET3_GATE_TEST) $(WORKLET3_QUEUE_TEST)
	./$(WORKLET3_GATE_TEST)
	./$(WORKLET3_QUEUE_TEST)

.PHONY: test-worklet3

