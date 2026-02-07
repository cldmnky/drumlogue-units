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

# Disable LCD functionality (not available on drumlogue)
UDEFS += -DNO_LCD


# Enable embedded ROM data
UDEFS += -DDRUMPLER_ROM_EMBEDDED
