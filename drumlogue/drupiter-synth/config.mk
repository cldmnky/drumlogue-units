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
  # For QEMU ARM testing, also define __QEMU_ARM__ to use chrono instead of hardware registers
  ifeq ($(__QEMU_ARM__),1)
    UDEFS += -D__QEMU_ARM__
  endif
else
  UDEFS =
endif

UDEFS += -DDRUPITER_MAX_VOICES=$(DRUPITER_MAX_VOICES)
UDEFS += -DUNISON_MAX_DETUNE=$(UNISON_MAX_DETUNE)

# Feature flags - NEON always enabled for ARM (Task 2.5)
UDEFS += -DUSE_NEON
UDEFS += -mfpu=neon
UDEFS += -mfloat-abi=hard

# Enable NEON-optimized DCO processing (requires USE_NEON)
UDEFS += -DNEON_DCO

# Enable PolyBLEP anti-aliasing
UDEFS += -DENABLE_POLYBLEP

# Performance optimizations
UDEFS += -O2
UDEFS += -ffast-math

# Optional: Enable debug profiling
# UDEFS += -DENABLE_PROFILING

# Debug output removed for production (Phase 1 complete)
# UDEFS += -DDEBUG

##############################################################################
# Linker Options
#
# Enable link-time garbage collection to remove unused code
USE_LINK_GC = yes
