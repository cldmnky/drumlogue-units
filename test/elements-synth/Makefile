# Makefile for elements-synth local test harness
# Compiles the DSP code for desktop testing

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
CXXFLAGS += -DTEST  # Enable TEST mode for desktop-compatible code paths
CXXFLAGS += -ffast-math  # Match ARM build for consistent behavior

# Debug build
ifdef DEBUG
CXXFLAGS := -std=c++17 -g -O0 -Wall -Wextra -Wno-unused-parameter -DTEST -DDEBUG
endif

# Detect platform and set library paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS - check for Homebrew
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifdef BREW_PREFIX
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS := -L$(BREW_PREFIX)/lib -lsndfile
    else
        LDFLAGS := -lsndfile
    endif
else
    # Linux
    LDFLAGS := -lsndfile -lm
endif

# Paths
REPO_ROOT := $(shell cd ../.. && pwd)
DSP_SRC := $(REPO_ROOT)/drumlogue/elements-synth
DSP_DIR := $(DSP_SRC)/dsp

# Include paths
INCLUDES := -I$(DSP_SRC)
INCLUDES += -I$(DSP_DIR)
INCLUDES += -I.

# Sources - elements-synth is header-only, so we only need main.cc
TEST_SRC := main.cc

# All sources
SOURCES := $(TEST_SRC)

# Output
TARGET := elements_synth_test

# Build directory
BUILD_DIR := build

# Object files
OBJECTS := $(patsubst %.cc,$(BUILD_DIR)/%.o,$(notdir $(SOURCES)))

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile rules
$(BUILD_DIR)/main.o: main.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.wav

# Install libsndfile on macOS
install-deps-macos:
	brew install libsndfile

# Install libsndfile on Linux
install-deps-linux:
	sudo apt-get install libsndfile1-dev

# Test targets
.PHONY: test test-preset test-notes test-analyze test-all

test: $(TARGET)
	./$(TARGET) test_output.wav --preset INIT --analyze

test-preset: $(TARGET)
	./$(TARGET) marimba.wav --preset MARIMBA --note 60 --analyze
	./$(TARGET) pluck.wav --preset PLUCK --note 48 --analyze
	./$(TARGET) bow.wav --preset BOW --duration 3 --analyze

test-notes: $(TARGET)
	./$(TARGET) scale.wav --preset MARIMBA --notes 60,62,64,65,67,69,71,72 --duration 4 --analyze

test-analyze: $(TARGET)
	@echo "Testing all presets for DSP stability..."
	@for i in 0 1 2 3 4 5 6 7; do \
		echo "Testing preset $$i..."; \
		./$(TARGET) test_preset_$$i.wav --preset $$i --duration 3 --analyze || exit 1; \
	done
	@echo "All presets passed!"

test-all: test test-preset test-notes test-analyze
	@echo "All tests completed successfully!"

# Debug run with gdb/lldb
debug: $(TARGET)
	lldb ./$(TARGET) -- test_debug.wav --preset INIT --analyze

help:
	@echo "Elements Synth Test Harness Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all             - Build the test harness (default)"
	@echo "  clean           - Remove build artifacts"
	@echo "  test            - Quick test with INIT preset"
	@echo "  test-preset     - Test several presets"
	@echo "  test-notes      - Test note sequences"
	@echo "  test-analyze    - Test all presets for stability"
	@echo "  test-all        - Run all tests"
	@echo "  debug           - Run under debugger"
	@echo "  install-deps-macos  - Install libsndfile on macOS"
	@echo "  install-deps-linux  - Install libsndfile on Linux"
	@echo ""
	@echo "Build options:"
	@echo "  make DEBUG=1    - Build with debug symbols"
