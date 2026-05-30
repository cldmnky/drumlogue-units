PROJECT := druteus
PROJECT_TYPE := synth

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc
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

# Preprocessor defines
UDEFS = -DUSE_NEON
UDEFS += -mfpu=neon
UDEFS += -mfloat-abi=hard
UDEFS += -ffast-math
