PROJECT := druteus
PROJECT_TYPE := synth

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc
CXXSRC += params.cc
CXXSRC += sf_loader.cc
CXXSRC += patch_engine.cc
CXXSRC += voice_engine.cc
CXXSRC += lfo_engine.cc
CXXSRC += dsp_chain.cc
CXXSRC += dsp_primitives.cc
CXXSRC += trance_gate.cc
CXXSRC += $(COMMON_SRC_PATH)/voice_allocator_core.cc

# Include paths (. ensures tsf.h and logue_fs.h are found in the project dir)
UINCDIR = .
UINCDIR += /repo/eurorack

# Common drumlogue utilities (absolute path required — unit lives outside SDK)
COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common

# Library paths and flags
ULIBDIR =
ULIBS   = -lm

# Warning suppressions for upstream eurorack code
USE_CWARN   = -W -Wall -Wextra -Wno-unused-local-typedefs -Wno-unused-parameter
USE_CXXWARN = -W -Wall -Wextra -Wno-ignored-qualifiers -Wno-unused-local-typedefs -Wno-unused-parameter

# Performance monitoring — enabled via: ./build.sh druteus build PERF_MON=1
# PERF_MON reads ARM DWT PMCCNTR (0xE0001004) which requires the debug
# unit to be enabled by firmware.  Default to OFF for production builds
# to avoid bus faults.  For QEMU: PERF_MON=1 __QEMU_ARM__=1
ifeq ($(PERF_MON),1)
  UDEFS = -DPERF_MON
  CXXSRC += $(COMMON_SRC_PATH)/perf_mon.cc
  ifeq ($(__QEMU_ARM__),1)
    UDEFS += -D__QEMU_ARM__
  endif
else
  UDEFS =
endif

UDEFS += -DUSE_NEON
UDEFS += -O2
