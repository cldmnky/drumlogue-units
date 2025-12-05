# Makefile for clouds-revfx local test harness
# Compiles the DSP code for desktop testing

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -DTEST  # Enable TEST mode for desktop-compatible code paths

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
    LDFLAGS := -lsndfile
endif

# Paths
REPO_ROOT := $(shell cd ../.. && pwd)
DSP_SRC := $(REPO_ROOT)/drumlogue/clouds-revfx
EURORACK := $(REPO_ROOT)/eurorack

# Include paths
INCLUDES := -I$(DSP_SRC)
INCLUDES += -I$(EURORACK)
INCLUDES += -I.

# Sources
# Test harness sources
TEST_SRC := main.cc

# DSP sources from clouds-revfx (excluding unit.cc which has drumlogue-specific code)
DSP_SRC_FILES := $(DSP_SRC)/clouds_fx.cc

# stmlib sources needed
STMLIB_SRC := $(EURORACK)/stmlib/utils/random.cc

# All sources
SOURCES := $(TEST_SRC) $(DSP_SRC_FILES) $(STMLIB_SRC)

# Output
TARGET := clouds_revfx_test

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

$(BUILD_DIR)/clouds_fx.o: $(DSP_SRC)/clouds_fx.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/random.o: $(EURORACK)/stmlib/utils/random.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET) fixtures/*.wav

# Test fixtures directory
fixtures:
	mkdir -p fixtures

# Generate test signals
generate-fixtures: $(TARGET) fixtures
	./$(TARGET) --generate-impulse fixtures/impulse.wav
	./$(TARGET) --generate-sine fixtures/sine_440.wav 440
	./$(TARGET) --generate-sine fixtures/sine_1000.wav 1000
	./$(TARGET) --generate-noise fixtures/noise.wav
	./$(TARGET) --generate-drums fixtures/drums.wav

# Run basic tests
test: $(TARGET) generate-fixtures
	@echo "=== Running DSP Tests ==="
	@echo ""
	@echo "Test 1: Process impulse through reverb"
	./$(TARGET) fixtures/impulse.wav fixtures/impulse_reverb.wav --dry-wet 100 --time 70
	@echo ""
	@echo "Test 2: Process drums with moderate reverb"
	./$(TARGET) fixtures/drums.wav fixtures/drums_reverb.wav --dry-wet 50 --time 50 --diffusion 60
	@echo ""
	@echo "Test 3: Process with texture/diffuser"
	./$(TARGET) fixtures/drums.wav fixtures/drums_texture.wav --dry-wet 60 --time 40 --texture 50
	@echo ""
	@echo "Test 4: Verify output files exist and have content"
	@for f in fixtures/impulse_reverb.wav fixtures/drums_reverb.wav fixtures/drums_texture.wav; do \
		if [ -f "$$f" ]; then \
			size=$$(wc -c < "$$f"); \
			if [ $$size -gt 1000 ]; then \
				echo "  ✓ $$f ($$size bytes)"; \
			else \
				echo "  ✗ $$f is too small ($$size bytes)"; \
				exit 1; \
			fi; \
		else \
			echo "  ✗ $$f not found"; \
			exit 1; \
		fi; \
	done
	@echo ""
	@echo "=== All tests passed ==="

# Quick sanity check
check: $(TARGET)
	./$(TARGET) --generate-impulse /tmp/test_impulse.wav
	./$(TARGET) /tmp/test_impulse.wav /tmp/test_output.wav --dry-wet 50
	@echo "Sanity check passed"
	@rm -f /tmp/test_impulse.wav /tmp/test_output.wav

# Test all presets
PRESETS := INIT HALL PLATE SHIMMER CLOUD FREEZE OCTAVER AMBIENT
SAMPLES := drums impulse

test-presets: $(TARGET) generate-fixtures
	@echo "=== Testing all presets ==="
	@mkdir -p fixtures/presets
	@for preset in $(PRESETS); do \
		echo ""; \
		echo "--- Preset: $$preset ---"; \
		for sample in $(SAMPLES); do \
			out="fixtures/presets/$${preset}_$${sample}.wav"; \
			./$(TARGET) "fixtures/$${sample}.wav" "$$out" --preset "$$preset" 2>&1 | tail -1; \
		done; \
	done
	@echo ""
	@echo "=== Verifying preset outputs ==="
	@count=0; \
	for f in fixtures/presets/*.wav; do \
		if [ -f "$$f" ]; then \
			size=$$(wc -c < "$$f"); \
			if [ $$size -gt 100000 ]; then \
				count=$$((count + 1)); \
			else \
				echo "  ✗ $$f is too small ($$size bytes)"; \
				exit 1; \
			fi; \
		fi; \
	done; \
	echo "  ✓ $$count preset output files generated successfully"
	@echo ""
	@echo "=== All preset tests passed ==="

# Full test suite
test-all: test test-presets
	@echo ""
	@echo "========================================"
	@echo "=== Full test suite completed ==="
	@echo "========================================"

.PHONY: all clean test check generate-fixtures fixtures test-presets test-all
