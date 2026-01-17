# Test harness for Pepege Synth DSP
# Builds DSP code for host machine testing

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST
CXXFLAGS += -I../../drumlogue/pepege-synth -I../../drumlogue/common -I../../eurorack/stmlib -I../../eurorack -I.

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
DSP_SOURCES := ../../drumlogue/pepege-synth/pepege_synth.h
DSP_SOURCES += ../../drumlogue/pepege-synth/resources/ppg_waves.cc
DSP_SOURCES += ../../drumlogue/common/perf_mon.cc

TEST_SOURCES := main.cc

TARGET := pepege_test

all: $(TARGET)

$(TARGET): $(TEST_SOURCES) $(DSP_SOURCES)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(TARGET) *.wav fixtures/

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