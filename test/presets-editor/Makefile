# Makefile for presets-editor Phase 1 (native unit host CLI)

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

CC  ?= clang
CXX ?= clang++
CFLAGS   := -std=c11 -O2 -Wall -Wextra -fPIC
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -fPIC
LDFLAGS  :=
LDLIBS   :=

ifeq ($(UNAME_S),Darwin)
  LDLIBS += 
else
  LDLIBS += -ldl
endif

# Enable NEON on ARM hosts
ifneq (,$(findstring arm64,$(UNAME_M)))
  CFLAGS   += -march=armv8-a+simd -D__ARM_NEON
  CXXFLAGS += -march=armv8-a+simd -D__ARM_NEON
endif

ROOT_DIR := $(abspath ../..)
SDK_DIR  := $(ROOT_DIR)/logue-sdk/platform/drumlogue
INC_DIRS := \
  -I$(SDK_DIR)/common \
  -I$(SDK_DIR)/inc \
  -I$(SDK_DIR)/inc/utils \
  -I$(SDK_DIR)/inc/dsp \
  -I./sdk \
  -I./core \
  -I./audio \
  -I./presets

SHARED_SRC_C := \
  sdk/runtime_stubs.c \
  core/unit_loader.c \
  audio/ring_buffer.c \
  audio/audio_engine.c \
  presets/preset_manager.c

SHARED_OBJS := $(patsubst %.c,build/%.o,$(SHARED_SRC_C))

SRC_C := $(SHARED_SRC_C) app/main.c
OBJS := $(patsubst %.c,build/%.o,$(SRC_C))

# PortAudio flags (best-effort detection)
PA_CFLAGS := $(shell pkg-config --cflags portaudio-2.0 2>/dev/null)
PA_LIBS   := $(shell pkg-config --libs portaudio-2.0 2>/dev/null)

# Homebrew fallback include/lib paths
ifeq ($(PA_CFLAGS),)
  ifneq ($(wildcard /opt/homebrew/include/portaudio.h),)
    PA_CFLAGS := -I/opt/homebrew/include
  endif
endif
ifeq ($(PA_LIBS),)
  ifneq ($(wildcard /opt/homebrew/lib/libportaudio.*),)
    PA_LIBS := -L/opt/homebrew/lib -lportaudio
  endif
endif

PORTAUDIO_PRESENT := $(if $(and $(PA_CFLAGS),$(PA_LIBS)),1,0)

ifeq ($(PORTAUDIO_PRESENT),1)
  CFLAGS   += $(PA_CFLAGS) -DPORTAUDIO_PRESENT
  CXXFLAGS += $(PA_CFLAGS) -DPORTAUDIO_PRESENT
  LDLIBS   += $(PA_LIBS)
else
  CFLAGS   += -DPORTAUDIO_MISSING
  CXXFLAGS += -DPORTAUDIO_MISSING
endif

BIN_DIR := bin
TARGET  := $(BIN_DIR)/presets-editor-cli

# GUI dependencies (SDL2 + ImGui)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)
IMGUI_CFLAGS := $(shell pkg-config --cflags imgui 2>/dev/null)
IMGUI_LIBS   := $(shell pkg-config --libs imgui 2>/dev/null)

ifeq ($(SDL_CFLAGS),)
  ifneq ($(wildcard /opt/homebrew/include/SDL2/SDL.h),)
    SDL_CFLAGS := -I/opt/homebrew/include/SDL2
  endif
endif
ifeq ($(SDL_LIBS),)
  ifneq ($(wildcard /opt/homebrew/lib/libSDL2.*),)
    SDL_LIBS := -L/opt/homebrew/lib -lSDL2
  endif
endif

ifeq ($(IMGUI_CFLAGS),)
  ifneq ($(wildcard /opt/homebrew/include/imgui.h),)
    IMGUI_CFLAGS := -I/opt/homebrew/include
  endif
endif
ifeq ($(IMGUI_LIBS),)
  ifneq ($(wildcard /opt/homebrew/lib/libimgui.*),)
    IMGUI_LIBS := -L/opt/homebrew/lib -limgui
  endif
endif

    # Local vendored ImGui fallback (expects sources under vendor/imgui)
    VENDOR_IMGUI_DIR := vendor/imgui
    VENDOR_IMGUI_SRC := \
      $(VENDOR_IMGUI_DIR)/imgui.cpp \
      $(VENDOR_IMGUI_DIR)/imgui_draw.cpp \
      $(VENDOR_IMGUI_DIR)/imgui_tables.cpp \
      $(VENDOR_IMGUI_DIR)/imgui_widgets.cpp \
      $(VENDOR_IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
      $(VENDOR_IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
    VENDOR_IMGUI_OBJS := $(patsubst %.cpp,build/%.o,$(VENDOR_IMGUI_SRC))

    ifneq (,$(wildcard $(VENDOR_IMGUI_DIR)/imgui.cpp))
      IMGUI_INC_LOCAL := -I$(VENDOR_IMGUI_DIR) -I$(VENDOR_IMGUI_DIR)/backends
    endif

ifeq ($(UNAME_S),Darwin)
  GL_LIBS := -framework OpenGL
else
  GL_LIBS := -lGL
endif

GUI_ENABLED := $(if $(and $(SDL_CFLAGS),$(SDL_LIBS),$(or $(IMGUI_CFLAGS),$(IMGUI_INC_LOCAL))),1,0)
GUI_TARGET := $(BIN_DIR)/presets-editor-gui
GUI_SRC := gui/main.cpp gui/imgui_app.cpp
GUI_OBJS := $(patsubst %.cpp,build/%.o,$(GUI_SRC))
GUI_INC := $(SDL_CFLAGS) $(IMGUI_CFLAGS) $(IMGUI_INC_LOCAL)

.PHONY: all clean build-unit

all: $(TARGET)
	@echo "Built $(TARGET)"

gui: $(GUI_TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

ifeq ($(GUI_ENABLED),1)
$(GUI_TARGET): $(SHARED_OBJS) $(GUI_OBJS) $(VENDOR_IMGUI_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(GUI_INC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(SDL_LIBS) $(IMGUI_LIBS) $(GL_LIBS)
else
$(GUI_TARGET):
	@echo "GUI deps missing (SDL2 + ImGui); install them then run 'make gui'"
endif

build/%.o: %.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC_DIRS) -c $< -o $@

build/%.o: %.cpp | build
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_DIRS) $(GUI_INC) -c $< -o $@

build:
	@mkdir -p build/app build/core build/sdk build/audio build/gui build/presets build/vendor/imgui/backends

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

clean:
	rm -rf build $(TARGET)

# Build a unit as native shared library
build-unit:
	./build-system/build-native.sh $(UNIT)
