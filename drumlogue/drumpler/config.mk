##############################################################################
# Project Configuration
#

PROJECT := drumpler_internal
PROJECT_TYPE := synth

##############################################################################
# Sources
#

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc rom_data.cc

# Emulator sources (JV-880 / Nuked-SC55)
CXXSRC += emulator/jv880_wrapper.cc
CXXSRC += emulator/mcu.cc
CXXSRC += emulator/mcu_opcodes.cc
CXXSRC += emulator/mcu_interrupt.cc
CXXSRC += emulator/mcu_timer.cc
CXXSRC += emulator/pcm.cc
CXXSRC += emulator/submcu.cc
CXXSRC += emulator/lcd_stub.cc

# List ASM source files here
ASMSRC = 

ASMXSRC = 

##############################################################################
# Include Paths
#

UINCDIR  = .
UINCDIR += emulator

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
# Macros
#

UDEFS = 

# Enable optimizations for ARM
UDEFS += -DARM_MATH_CM4
UDEFS += -D__FPU_PRESENT=1

# Enable NEON SIMD helpers for ARM builds
UDEFS += -DUSE_NEON

# Disable LCD functionality (not available on drumlogue)
UDEFS += -DNO_LCD


# Enable embedded ROM data
UDEFS += -DDRUMPLER_ROM_EMBEDDED

# Enable performance monitoring via command line: ./build.sh drumpler PERF_MON=1
ifeq ($(PERF_MON),1)
	UDEFS += -DPERF_MON
	CXXSRC += ../common/perf_mon.cc
	# For QEMU ARM testing, also define __QEMU_ARM__ to use chrono instead of hardware registers
	ifeq ($(__QEMU_ARM__),1)
		UDEFS += -D__QEMU_ARM__
	endif
endif

ifeq ($(DEBUG),1)
	UDEFS += -DDEBUG
endif
