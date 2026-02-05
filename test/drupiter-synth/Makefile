# Test harness for Drupiter Synth DSP
# Builds DSP code for host machine testing

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST
CXXFLAGS += -I../../drumlogue/drupiter-synth -I../../drumlogue/common -I../../eurorack/stmlib -I../../eurorack -I.

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
DSP_SOURCES += ../../drumlogue/drupiter-synth/dsp/jupiter_vcf.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/dsp/jupiter_env.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/dsp/jupiter_lfo.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/dsp/voice_allocator.cc
DSP_SOURCES += ../../drumlogue/common/voice_allocator_core.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/dsp/unison_oscillator.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/polyphonic_renderer.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/mono_renderer.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/unison_renderer.cc
DSP_SOURCES += ../../drumlogue/drupiter-synth/drupiter_synth.cc
DSP_SOURCES += ../../drumlogue/common/perf_mon.cc

TEST_SOURCES := main.cc
PERF_SOURCES := perf_test.cc

TARGET := drupiter_test
PERF_TARGET := perf_test

all: $(TARGET) $(PERF_TARGET)

$(TARGET): $(TEST_SOURCES) $(DSP_SOURCES)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(PERF_TARGET): $(PERF_SOURCES) $(DSP_SOURCES)
	$(CXX) $(CXXFLAGS) -DPERF_MON $^ $(LDFLAGS) -o $@

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

perf-test: $(PERF_TARGET)
	@echo "=== Running Performance Tests ==="
	@echo "Note: Build with PERF_MON=1 for accurate results"
	@echo "Example: make clean && make PERF_MON=1 perf-test"
	./$(PERF_TARGET)

.PHONY: all clean test test-single test-multi generate-fixtures perf-test