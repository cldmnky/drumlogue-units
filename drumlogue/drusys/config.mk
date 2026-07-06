PROJECT := drusys
PROJECT_TYPE := synth

CSRC = header.c
CXXSRC = unit.cc

UINCDIR = .

COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common

ULIBDIR =
ULIBS   = -lm

USE_CWARN   = -W -Wall -Wextra -Wno-unused-local-typedefs -Wno-unused-parameter
USE_CXXWARN = -W -Wall -Wextra -Wno-ignored-qualifiers -Wno-unused-local-typedefs -Wno-unused-parameter

UDEFS = -DUSE_NEON
UDEFS += -O2
