# Makefile for elements-synth local test harness
# Compiles the DSP code for desktop testing

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
CXXFLAGS += -DTEST  # Enable TEST mode for desktop-compatible code paths
CXXFLAGS += -ffast-math  # Match ARM build for consistent behavior

# Default to LIGHTWEIGHT mode to match drumlogue build
# Override with FULL=1 to build with filter and LFO
ifndef FULL
CXXFLAGS += -DELEMENTS_LIGHTWEIGHT
BUILD_MODE := lightweight
else
BUILD_MODE := full
endif

# Debug build
ifdef DEBUG
CXXFLAGS := -std=c++17 -g -O0 -Wall -Wextra -Wno-unused-parameter -DTEST -DDEBUG
ifndef FULL
CXXFLAGS += -DELEMENTS_LIGHTWEIGHT
endif
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
DSP_SRC := $(REPO_ROOT)/drumlogue/elementish-synth
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
.PHONY: test test-preset test-notes test-pattern test-analyze test-compare test-profile test-all test-quick

# Quick sanity test
test: $(TARGET)
	@echo "=== Quick Test ($(BUILD_MODE) mode) ==="
	./$(TARGET) test_output.wav --preset INIT --analyze

# Test individual presets with varying parameters
test-preset: $(TARGET)
	@echo "=== Preset Tests with Parameter Variations ($(BUILD_MODE) mode) ==="
	@echo ""
	@echo "--- MARIMBA: C major scale with varying velocity ---"
	./$(TARGET) marimba_scale.wav --preset MARIMBA --notes 60,64,67,72,67,64,60,55 --duration 4 --analyze
	@echo ""
	@echo "--- PLUCK: Low bass with bright settings ---"
	./$(TARGET) pluck_bass.wav --preset PLUCK --note 36 --brightness 40 --damping -30 --duration 3 --analyze
	@echo ""
	@echo "--- BOW: Sustained note with geometry sweep ---"
	./$(TARGET) bow_sustain.wav --preset BOW --note 60 --geometry 30 --duration 4 --analyze
	@echo ""
	@echo "--- STRING: Chord progression ---"
	./$(TARGET) string_chord.wav --preset STRING --notes 48,52,55,60,55,52,48 --duration 5 --analyze
	@echo ""
	@echo "--- MSTRING: Ethereal high notes ---"
	./$(TARGET) mstring_high.wav --preset MSTRING --notes 72,74,76,79,84 --damping -40 --duration 4 --analyze

# Test note sequences - musical scales
test-notes: $(TARGET)
	@echo "=== Note Sequence Tests ($(BUILD_MODE) mode) ==="
	@echo ""
	@echo "--- C Major Scale (ascending + descending) ---"
	./$(TARGET) scale_cmaj.wav --preset MARIMBA --notes 60,62,64,65,67,69,71,72,71,69,67,65,64,62,60 --duration 6 --analyze
	@echo ""
	@echo "--- Chromatic Scale ---"
	./$(TARGET) scale_chrom.wav --preset VIBES --notes 48,49,50,51,52,53,54,55,56,57,58,59,60 --duration 5 --analyze
	@echo ""
	@echo "--- Pentatonic Pattern ---"
	./$(TARGET) scale_penta.wav --preset PLUCK --notes 60,62,64,67,69,72,69,67,64,62,60 --duration 4 --analyze

# Test a full 2-bar musical pattern (16 steps at 120 BPM = 8 seconds)
test-pattern: $(TARGET)
	@echo "=== 2-Bar Pattern Tests ($(BUILD_MODE) mode) ==="
	@echo "--- 120 BPM, 4/4 time, 2 bars = 8 seconds ---"
	@echo ""
	@echo "--- MARIMBA: Melodic pattern ---"
	./$(TARGET) pattern_marimba.wav --preset MARIMBA \
		--notes 60,60,67,67,69,69,67,0,65,65,64,64,62,62,60,0 \
		--duration 8 --analyze
	@echo ""
	@echo "--- VIBES: Jazz chord voicings ---"
	./$(TARGET) pattern_vibes.wav --preset VIBES --geometry 20 --brightness 30 \
		--notes 60,64,67,72,60,63,67,70,60,65,69,72,60,64,67,71 \
		--duration 8 --analyze
	@echo ""
	@echo "--- BOW: Drone with variations ---"
	./$(TARGET) pattern_bow.wav --preset BOW --geometry -20 --brightness 40 \
		--notes 48,48,48,48,55,55,55,55,53,53,53,53,48,48,48,48 \
		--duration 8 --analyze
	@echo ""
	@echo "--- STRING: Plucked bass line ---"
	./$(TARGET) pattern_string.wav --preset STRING --damping -25 \
		--notes 36,36,43,36,38,38,45,38,41,41,48,41,43,43,48,36 \
		--duration 8 --analyze
	@echo ""
	@echo "--- MSTRING: Ambient pad swells ---"
	./$(TARGET) pattern_mstring.wav --preset MSTRING --damping -50 --brightness 20 \
		--notes 60,60,60,60,67,67,67,67,65,65,65,65,62,62,62,62 \
		--duration 8 --analyze

# Test all presets for DSP stability with parameter extremes
test-analyze: $(TARGET)
	@echo "=== Stability Tests - All Presets ($(BUILD_MODE) mode) ==="
	@for i in 0 1 2 3 4 5 6 7; do \
		echo "Testing preset $$i with default params..."; \
		./$(TARGET) test_preset_$$i.wav --preset $$i --duration 3 --analyze || exit 1; \
	done
	@echo ""
	@echo "--- Testing extreme parameter values ---"
	./$(TARGET) test_extreme_bright.wav --preset INIT --brightness 63 --damping -63 --duration 2 --analyze || exit 1
	./$(TARGET) test_extreme_dark.wav --preset INIT --brightness -63 --damping 63 --duration 2 --analyze || exit 1
	./$(TARGET) test_extreme_geo.wav --preset INIT --geometry 63 --duration 2 --analyze || exit 1
	@echo "All stability tests passed!"

# Mode comparison tests
test-compare: $(TARGET)
	@echo "=== Resonator Mode Comparison ($(BUILD_MODE) mode) ==="
	@echo ""
	@echo "--- Single Note Comparison (C4) ---"
	./$(TARGET) --compare-modes --note 60 --duration 2
	@echo ""
	@echo "--- Multi-Note Comparison (full range) ---"
	./$(TARGET) --multi-note 36,48,60,72,84 --duration 1.5
	@echo ""
	@echo "--- Save comparison WAVs ---"
	./$(TARGET) comparison --compare-modes --note 60 --save-comparison --duration 2

# CPU profiling tests
test-profile: $(TARGET)
	@echo "=== CPU Profiling ($(BUILD_MODE) mode) ==="
	@echo ""
	@echo "--- MODAL resonator profile ---"
	./$(TARGET) profile_modal.wav --preset INIT --model 0 --duration 2 --profile
	@echo ""
	@echo "--- STRING resonator profile ---"
	./$(TARGET) profile_string.wav --preset STRING --duration 2 --profile
	@echo ""
	@echo "--- MSTRING resonator profile ---"
	./$(TARGET) profile_mstring.wav --preset MSTRING --duration 2 --profile
	@echo ""
	@echo "--- Complex preset (BOW) profile ---"
	./$(TARGET) profile_bow.wav --preset BOW --duration 3 --profile

# Quick test suite (fast, essential tests only)
test-quick: $(TARGET)
	@echo "=== Quick Test Suite ($(BUILD_MODE) mode) ==="
	./$(TARGET) test_init.wav --preset INIT --analyze
	./$(TARGET) test_string.wav --preset STRING --analyze
	./$(TARGET) --compare-modes --note 60 --duration 1
	@echo "Quick tests passed!"

# Comprehensive test suite
test-all: $(TARGET)
	@echo "╔══════════════════════════════════════════════════════════════════════════════╗"
	@echo "║              ELEMENTS SYNTH COMPREHENSIVE TEST SUITE                         ║"
	@echo "║                        Build Mode: $(BUILD_MODE)                                      ║"
	@echo "╚══════════════════════════════════════════════════════════════════════════════╝"
	@echo ""
	@$(MAKE) test-analyze
	@echo ""
	@$(MAKE) test-preset
	@echo ""
	@$(MAKE) test-notes
	@echo ""
	@$(MAKE) test-pattern
	@echo ""
	@$(MAKE) test-compare
	@echo ""
	@$(MAKE) test-profile
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════════════════════╗"
	@echo "║                    ALL TESTS COMPLETED SUCCESSFULLY!                         ║"
	@echo "╚══════════════════════════════════════════════════════════════════════════════╝"

# Debug run with gdb/lldb
debug: $(TARGET)
	lldb ./$(TARGET) -- test_debug.wav --preset INIT --analyze

help:
	@echo "Elements Synth Test Harness Build System"
	@echo ""
	@echo "Build Targets:"
	@echo "  all             - Build the test harness (default: lightweight mode)"
	@echo "  clean           - Remove build artifacts"
	@echo ""
	@echo "Test Targets:"
	@echo "  test            - Quick test with INIT preset"
	@echo "  test-quick      - Fast essential tests only"
	@echo "  test-preset     - Test presets with parameter variations"
	@echo "  test-notes      - Test musical scales (major, chromatic, pentatonic)"
	@echo "  test-pattern    - Test 2-bar musical patterns (8 seconds @ 120 BPM)"
	@echo "  test-analyze    - Test all presets for DSP stability"
	@echo "  test-compare    - Compare resonator modes (MODAL/STRING/MSTRING)"
	@echo "  test-profile    - Run CPU profiling on different modes"
	@echo "  test-all        - Run comprehensive test suite"
	@echo "  debug           - Run under debugger"
	@echo ""
	@echo "Build Options:"
	@echo "  make            - Build lightweight mode (matches drumlogue)"
	@echo "  make FULL=1     - Build with filter and LFO"
	@echo "  make DEBUG=1    - Build with debug symbols"
	@echo ""
	@echo "Install Dependencies:"
	@echo "  make install-deps-macos  - Install libsndfile on macOS"
	@echo "  make install-deps-linux  - Install libsndfile on Linux"
	@echo ""
	@echo "Current Build Mode: $(BUILD_MODE)"
