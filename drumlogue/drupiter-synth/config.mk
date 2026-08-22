##############################################################################
# Project Configuration
#

PROJECT := drupiter_synth
PROJECT_TYPE := synth

##############################################################################
# Sources
#

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc
CXXSRC += drupiter_synth.cc

# Renderer sources (extracted from drupiter_synth.cc)
CXXSRC += polyphonic_renderer.cc
CXXSRC += mono_renderer.cc
CXXSRC += unison_renderer.cc

# Common utilities (conditionally compiled based on PERF_MON)
# CXXSRC += ../common/perf_mon.cc  # Only when PERF_MON is defined

# DSP component sources
CXXSRC += dsp/jupiter_dco.cc
CXXSRC += dsp/jupiter_vcf.cc
CXXSRC += dsp/jupiter_env.cc
CXXSRC += dsp/jupiter_lfo.cc
CXXSRC += dsp/voice_allocator.cc   # Drupiter-specific voice allocator
CXXSRC += dsp/unison_oscillator.cc

# Common voice allocator core (from drumlogue/common)
CXXSRC += ../common/voice_allocator_core.cc

# List ASM source files here
ASMSRC = 

ASMXSRC = 

##############################################################################
# Include Paths
#
# Note: We need to explicitly set COMMON_INC_PATH because our project lives
# outside the SDK directory and is symlinked in during build. realpath would
# resolve through the symlink to the wrong location.

COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common

# Include project directory and dsp subdirectory
UINCDIR  = .
UINCDIR += dsp

##############################################################################
# Library Paths
#

ULIBDIR = 

##############################################################################
# Libraries
#

ULIBS  = -lm
ULIBS += -lc

##############################################################################
# Additional defines
#

# === Synthesis Mode Configuration (Hoover v2.0) ===
# NOTE: Mode selection is now RUNTIME via parameter (no recompilation needed)
# Voice counts still compile-time (for buffer allocation)
UNISON_VOICES ?= 4
DRUPITER_MAX_VOICES ?= 4

# Unison detune range (cents)
UNISON_MAX_DETUNE ?= 50


# Build defines - Synthesis mode
# Enable performance monitoring via command line: ./build.sh drupiter-synth PERF_MON=1
ifeq ($(PERF_MON),1)
  UDEFS = -DPERF_MON
  CXXSRC += ../common/perf_mon.cc
  # Keep symbols so the QEMU ARM host can read PERF_MON counters via dlsym
  # (the SDK Makefile strips unless DEBUG is set, and DEBUG adds -DDEBUG prints)
  override STRIP := true
  # For QEMU ARM testing, also define __QEMU_ARM__ to use chrono instead of hardware registers
  ifeq ($(__QEMU_ARM__),1)
    UDEFS += -D__QEMU_ARM__
  endif
else
  UDEFS =
endif

UDEFS += -DDRUPITER_MAX_VOICES=$(DRUPITER_MAX_VOICES)
UDEFS += -DUNISON_MAX_DETUNE=$(UNISON_MAX_DETUNE)

# Feature flags - NEON enabled for ARM (Cortex-A7 vector unit)
UDEFS += -DUSE_NEON

# Enable NEON-optimized DCO processing (requires USE_NEON)
UDEFS += -DNEON_DCO

# Enable PolyBLEP anti-aliasing
UDEFS += -DENABLE_POLYBLEP

# Performance optimizations
# NOTE: The SDK Makefile already selects -march=armv7-a -mtune=cortex-a7
# -mfloat-abi=hard -mfpu=neon-vfpv4 and a default -Os. Do NOT append -mfpu,
# -mfloat-abi, -O levels or -ffast-math through UDEFS here: UDEFS land at the
# END of the compiler invocation and silently override the Makefile's
# architecture flags (this previously downgraded neon-vfpv4 -> plain neon).
OPTIM = -O2

# Bind unit-internal symbols locally so LTO can inline across translation
# units. Previously every internal call went through the PLT (~205 exported
# symbols), adding an indirect call+load per oscillator/filter/envelope call
# in the per-sample path. SDK callbacks (unit_*) keep default visibility via
# __unit_callback/used attributes.
USE_COPT  += -fno-semantic-interposition
USE_CXXOPT += -fno-semantic-interposition
# Cross-TU calls also bind locally (functions only; data relocations untouched)
USE_LDOPT += -Bsymbolic-functions

# Optional: Enable debug profiling
# UDEFS += -DENABLE_PROFILING

# Debug output removed for production (Phase 1 complete)
# UDEFS += -DDEBUG

##############################################################################
# Linker Options
#
# Enable link-time garbage collection to remove unused code
USE_LINK_GC = yes
