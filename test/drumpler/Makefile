# Test harness for Drumpler (JV-880 emulator)
# Builds emulator code for host machine testing

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST -DDEBUG
CXXFLAGS += -I../../drumlogue/drumpler -I../../drumlogue/drumpler/emulator \
  -I../../drumlogue/common -I../../eurorack/stmlib -I../../eurorack -I.

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
DSP_SOURCES := ../../drumlogue/drumpler/emulator/jv880_wrapper.cc
DSP_SOURCES += ../../drumlogue/drumpler/emulator/mcu.cc
DSP_SOURCES += ../../drumlogue/drumpler/emulator/mcu_opcodes.cc
DSP_SOURCES += ../../drumlogue/drumpler/emulator/mcu_interrupt.cc
DSP_SOURCES += ../../drumlogue/drumpler/emulator/mcu_timer.cc
DSP_SOURCES += ../../drumlogue/drumpler/emulator/pcm.cc
DSP_SOURCES += ../../drumlogue/drumpler/emulator/submcu.cc
DSP_SOURCES += ../../drumlogue/drumpler/emulator/lcd_stub.cc

TEST_SOURCES := main.cc

TARGET := drumpler_test

all: $(TARGET)

$(TARGET): $(TEST_SOURCES) $(DSP_SOURCES)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(TARGET) fixtures/ output.wav

# Test targets
fixtures:
	mkdir -p fixtures

test: $(TARGET) fixtures
	@echo "=== Rendering JV-880 Test Output ==="
	@mkdir -p fixtures
	./$(TARGET) --rom ../../drumlogue/drumpler/resources/jv880_base_srjv80-10.bin \\
	  --out fixtures/drumpler_test.wav --seconds 2.0 --note 60 --velocity 100 --program 0
	@echo "✓ Rendered fixtures/drumpler_test.wav"

.PHONY: all clean fixtures test
