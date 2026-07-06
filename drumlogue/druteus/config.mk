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

# Preprocessor defines.  USE_NEON enables NEON codecs in common/neon_dsp.h
# and simd_utils.h.  PERF_MON enables cycle-counting instrumentation for
# profiling render cost on hardware.
# NOTE: -mfpu=neon -mfloat-abi=hard are already set by the SDK Makefile
# (OPT += -mfpu=neon-vfpv4 -mfloat-abi=hard); do NOT repeat them here.
# NOTE: -ffast-math was removed — it enables -ffinite-math-only which
# replaces standard powf/expf/log with __finite variants that break TSF's
# IEEE assumptions (e.g. log(0) in envelope/gain calculations).
UDEFS = -DUSE_NEON
UDEFS += -DPERF_MON
CXXSRC += $(COMMON_SRC_PATH)/perf_mon.cc
UDEFS += -O2

# Conditional QEMU ARM override — test-unit.sh passes __QEMU_ARM__=1 so
# perf_mon.h uses std::chrono instead of the real ARM DWT register.
ifeq ($(__QEMU_ARM__),1)
  UDEFS += -D__QEMU_ARM__
endif
