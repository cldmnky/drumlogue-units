# Test harness for Drupiter Synth DSP
# Builds DSP code for host machine testing

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST
CXXFLAGS += -I../../drumlogue/drupiter-synth -I../../drumlogue/common -I../../eurorack/stmlib -I../../eurorack

# Platform detection for libsndfile
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifdef BREW_PREFIX
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS := -L$(BREW_PREFIX)/lib -lsndfile
    else
        LDFLAGS := -lsndfile
    endif
else
    LDFLAGS := -lsndfile
endif

# Source files
DSP_SOURCES := ../../drumlogue/drupiter-synth/dsp/jupiter_dco.cc
TEST_SOURCES := main.cc

TARGET := drupiter_test

all: $(TARGET)

$(TARGET): $(TEST_SOURCES) $(DSP_SOURCES)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(TARGET) *.wav fixtures/

# Test targets
fixtures:
	mkdir -p fixtures

generate-fixtures: $(TARGET) fixtures
	./$(TARGET) generate-sine --freq 440 --output fixtures/sine_440.wav --duration 1.0

test-single: $(TARGET) fixtures
	@echo "=== Testing Single Oscillator ==="
	./$(TARGET) single-osc --freq 440 --wave saw --output fixtures/single_saw.wav
	./$(TARGET) single-osc --freq 440 --wave square --output fixtures/single_square.wav
	./$(TARGET) single-osc --freq 440 --wave sine --output fixtures/single_sine.wav

test-multi: $(TARGET) fixtures
	@echo "=== Testing Multiple Oscillators ==="
	./$(TARGET) multi-osc --freq 220 --output fixtures/multi_220.wav --duration 2.0

test: test-single test-multi
	@echo "=== All tests completed ==="
	@echo "Check fixtures/ directory for output WAV files"

.PHONY: all clean test test-single test-multi generate-fixtures