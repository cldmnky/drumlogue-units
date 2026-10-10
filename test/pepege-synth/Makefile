# Test harness for Pepege Synth DSP
# Builds DSP code for host machine testing

CXX := g++
INCLUDES := -I../../drumlogue/pepege-synth -I../../drumlogue/common -I../../eurorack/stmlib -I../../eurorack -I.
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST
CXXFLAGS += $(INCLUDES)

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

# Source files (headers listed separately: g++ would otherwise try to
# precompile the header as a translation unit of its own)
DSP_SOURCES := ../../drumlogue/pepege-synth/resources/ppg_waves.cc
DSP_SOURCES += ../../drumlogue/common/perf_mon.cc
DSP_HEADERS := ../../drumlogue/pepege-synth/pepege_synth.h

TEST_SOURCES := main.cc

TARGET := pepege_test

# Callback benchmark (no libsndfile needed). The unit is built with
# -Os -ffast-math -fno-math-errno, so the benchmark mirrors that by default;
# override to measure the effect of a different optimization level:
#   make perf-test PERF_OPT=-O2
PERF_TARGET := perf_test
PERF_SOURCES := perf_test.cc
PERF_OPT ?= -Os

all: $(TARGET)

$(TARGET): $(TEST_SOURCES) $(DSP_SOURCES) $(DSP_HEADERS)
	$(CXX) $(CXXFLAGS) $(TEST_SOURCES) $(DSP_SOURCES) $(LDFLAGS) -o $@

$(PERF_TARGET): $(PERF_SOURCES) $(DSP_SOURCES) $(DSP_HEADERS) FORCE
	$(CXX) -std=c++17 -Wall -Wextra $(INCLUDES) $(PERF_OPT) -ffast-math -fno-math-errno -fno-stack-protector -DTEST $(PERF_SOURCES) $(DSP_SOURCES) -o $@

# Always rebuild: PERF_OPT is not a file, so without this a changed flag would
# silently reuse the previous binary (make perf-test PERF_OPT=-O2).
FORCE:

clean:
	rm -rf $(TARGET) $(PERF_TARGET) *.wav *.f32 fixtures/

# Test targets
fixtures:
	mkdir -p fixtures

generate-fixtures: $(TARGET) fixtures
	./$(TARGET) --generate-impulse fixtures/impulse.wav
	./$(TARGET) --generate-sine fixtures/sine_440.wav 440

test: $(TARGET) generate-fixtures
	@echo "=== Running Pepege Synth DSP Tests ==="
	./$(TARGET) fixtures/impulse.wav fixtures/out_impulse.wav
	./$(TARGET) fixtures/sine_440.wav fixtures/out_sine.wav --param1 50
	@echo "✓ All tests passed"

# Callback benchmark; PERF_OPT selects the optimization level (default -Os)
perf-test: $(PERF_TARGET)
	./$(PERF_TARGET)

.PHONY: all clean fixtures generate-fixtures test perf-test