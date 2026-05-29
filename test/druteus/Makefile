CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifdef BREW_PREFIX
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS  := -L$(BREW_PREFIX)/lib -lsndfile
    else
        LDFLAGS  := -lsndfile
    endif
else
    LDFLAGS := -lsndfile
endif

REPO_ROOT := $(shell cd ../.. && pwd)
DSP_SRC   := $(REPO_ROOT)/drumlogue/druteus
INCLUDES  := -I$(DSP_SRC) -I.

TARGET    := druteus_test
SOURCES   := main.cc

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) fixtures/*.wav

fixtures:
	mkdir -p fixtures

generate-fixtures: $(TARGET) fixtures
	./$< --generate-impulse fixtures/impulse.wav
	./$< --generate-sine fixtures/sine_440.wav 440
	./$< --generate-noise fixtures/noise.wav

test: $(TARGET) generate-fixtures
	@echo "=== Impulse response ==="
	./$< fixtures/impulse.wav fixtures/out_impulse.wav
	@echo "=== Sine wave ==="
	./$< fixtures/sine_440.wav fixtures/out_sine.wav
	@echo "=== All tests passed ==="

.PHONY: all clean test generate-fixtures
