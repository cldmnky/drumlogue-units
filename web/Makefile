# Web emulator build: compiles a drumlogue unit's sources to WebAssembly.
#
#   make                       # builds www/units/pepege-synth.wasm
#   make UNIT=clouds-revfx     # any unit under drumlogue/
#   make serve                 # serves www/ on http://localhost:8000
#   make clean
#
# clouds-revfx (and any unit that reuses Mutable Instruments code) needs the
# eurorack tree; pass EURORACK=/path/to/eurorack if it is not ../eurorack.
#
# Needs an activated emsdk; point EMSCRIPTEN at it if it is not ~/emsdk.
# The emsdk that ships with the logue SDK (logue-sdk/tools/emsdk) works:
#   ./emsdk install latest && ./emsdk activate latest

EMSCRIPTEN ?= $(HOME)/emsdk
UNIT ?= pepege-synth
SDK ?= ../logue-sdk
SDK_COMMON := $(SDK)/platform/drumlogue/common
UNIT_DIR := ../drumlogue/$(UNIT)
OUT_DIR := www/units
OUT := $(OUT_DIR)/$(UNIT).wasm
EMXX := $(EMSCRIPTEN)/upstream/emscripten/em++

# Units that reuse Mutable Instruments DSP (clouds-revfx, ...) point at the
# eurorack tree where the ARM build container mounts it: /repo/eurorack. A
# worktree has no submodules, so point EURORACK at a checkout (or mount one at
# /src/eurorack for the container recipe in README.md).
EURORACK ?= ../eurorack
EURORACK_MOUNT := /repo/eurorack

# The unit's own config tells us its sources and defines.
include $(UNIT_DIR)/config.mk
# USE_NEON and the unit-level switches that depend on it have to go: there is
# no NEON in WebAssembly and every unit has scalar fallbacks. Dropping only
# USE_NEON is not enough - drupiter's NEON_DCO path is guarded by
# "#ifdef NEON_DCO" alone in places, so leaving it defined breaks the build.
WASM_DEFS := $(filter-out -DUSE_NEON -DNEON_DCO,$(UDEFS))

# The units' config puts COMMON_INC_PATH/COMMON_SRC_PATH at
# /workspace/drumlogue/common, the ARM build container's copy of the repo's
# drumlogue/common. Use the checkout's own copy instead.
COMMON_DIR := ../drumlogue/common
ifneq ($(wildcard $(COMMON_DIR)),)
COMMON_PATH := $(abspath $(COMMON_DIR))
ifneq ($(filter /%,$(COMMON_INC_PATH)),)
UINCDIR := $(patsubst $(COMMON_INC_PATH)%,$(COMMON_PATH)%,$(UINCDIR))
CXXSRC := $(patsubst $(COMMON_SRC_PATH)%,$(COMMON_PATH)%,$(CXXSRC))
CSRC := $(patsubst $(COMMON_SRC_PATH)%,$(COMMON_PATH)%,$(CSRC))
endif
endif

# Rewrite the ARM container's eurorack mount point to the local checkout. The
# unit's config sets UINCDIR/CXXSRC to $(EURORACKDIR)... recursively, so this
# has to happen after the include, on the expanded values.
ifneq ($(wildcard $(EURORACK)),)
EURORACK_PATH := $(abspath $(EURORACK))
UINCDIR := $(patsubst $(EURORACK_MOUNT)%,$(EURORACK_PATH)%,$(UINCDIR))
CXXSRC := $(patsubst $(EURORACK_MOUNT)%,$(EURORACK_PATH)%,$(CXXSRC))
CSRC := $(patsubst $(EURORACK_MOUNT)%,$(EURORACK_PATH)%,$(CSRC))

# Mutable Instruments' stmlib uses ARM inline asm (ssat/usat/vsqrt.f32) in its
# clipping and sqrt helpers unless TEST selects the portable ones - the same
# switch its own desktop builds use. WebAssembly has no ARM asm, so any unit
# that pulls in the eurorack tree is built the way the desktop tests build it.
ifneq ($(strip $(filter $(EURORACK_PATH)%,$(CXXSRC) $(CSRC) $(UINCDIR))),)
WASM_DEFS += -DTEST
endif
endif

# Sources in CSRC/CXXSRC may be unit-relative (unit.cc), relative to the unit
# (../common/voice_allocator_core.cc) or absolute (eurorack, common); only the
# first kind needs the unit's directory in front of it. Include paths follow
# the same rule. _unit_base.c is the SDK's set of weak fallbacks: a unit such
# as an effect need not implement note_on/presets, and the shim calls them.
INCLUDES := -I$(UNIT_DIR) -I$(SDK_COMMON) -I$(COMMON_DIR) \
            $(foreach dir,$(UINCDIR),$(if $(filter /%,$(dir)),-I$(dir),-I$(UNIT_DIR)/$(dir)))
SOURCES := $(foreach src,$(CXXSRC) $(CSRC),\
             $(if $(filter /%,$(src)),$(src),$(UNIT_DIR)/$(src))) \
           $(SDK_COMMON)/_unit_base.c src/unit_shim.c

EXPORTS := _host_init,_host_teardown,_host_render,_host_out_ptr,_host_in_ptr \
           ,_host_channels,_host_note_on,_host_note_off,_host_all_note_off \
           ,_host_set_param,_host_get_param,_host_load_preset,_host_get_preset_index \
           ,_host_set_tempo,_host_pitch_bend,_host_reset,_host_param_str \
           ,_host_string_ptr,_host_manifest,_host_manifest_ptr,_malloc,_free

CFLAGS := -O2 -ffast-math -fno-exceptions -fno-rtti $(WASM_DEFS) $(INCLUDES)
LDFLAGS := -sSTANDALONE_WASM=1 -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=16777216 \
           -sERROR_ON_UNDEFINED_SYMBOLS=1 --no-entry \
           -sEXPORTED_FUNCTIONS="$(EXPORTS)"

all: $(OUT)

$(OUT): $(SOURCES) $(UNIT_DIR)/config.mk Makefile
	@mkdir -p $(OUT_DIR)
	$(EMXX) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@
	@$(MAKE) --no-print-directory index

# The page opens the first unit in index.json when the URL has no ?unit= (see
# www/app.js's "params.get('unit') || units[0]"), so the reference synth comes
# first and the rest follow alphabetically rather than the other way round.
DEFAULT_UNIT ?= pepege-synth

index:
	@cd $(OUT_DIR) && { if [ -f $(DEFAULT_UNIT).wasm ]; then echo $(DEFAULT_UNIT); fi; \
	  ls *.wasm 2>/dev/null | sed 's/\.wasm$$//' | grep -vx '$(DEFAULT_UNIT)' || true; } \
	  | python3 -c 'import json,sys; print(json.dumps([l.strip() for l in sys.stdin if l.strip()]))' \
	  > index.json
	@echo "units: $$(cat $(OUT_DIR)/index.json 2>/dev/null)"

serve:
	@echo "http://localhost:8000/"
	@python3 -m http.server 8000 --directory www

# Serve the built site from a ConfigMap on OpenShift; see deploy/README.md.
# DEPLOY_UNITS must match the volume items in deploy/drumlogue-web.yaml.
# The ConfigMap is applied server-side: a client-side apply would keep the whole
# object in an annotation, and a few wasm modules exceed its 256KiB limit.
NAMESPACE ?= pi-pocket
DEPLOY_UNITS ?= pepege-synth clouds-revfx elementish-synth drupiter-synth
DEPLOY_WASM := $(foreach u,$(DEPLOY_UNITS),--from-file=unit-$(u)=www/units/$(u).wasm)
deploy:
	kubectl create configmap drumlogue-web -n $(NAMESPACE) \
	  --from-file=index.html=www/index.html \
	  --from-file=app.js=www/app.js \
	  --from-file=unit-host.js=www/unit-host.js \
	  --from-file=worklet.js=www/worklet.js \
	  --from-file=styles.css=www/styles.css \
	  --from-file=units-index.json=www/units/index.json \
	  $(DEPLOY_WASM) \
	  --dry-run=client -o yaml | kubectl apply --server-side -f -
	kubectl apply -f ../deploy/drumlogue-web.yaml -n $(NAMESPACE)
	kubectl rollout restart deployment/drumlogue-web -n $(NAMESPACE)
	kubectl get route drumlogue-web -n $(NAMESPACE) \
	  -o jsonpath='https://{.spec.host}{"\n"}'

clean:
	rm -rf $(OUT_DIR)

.PHONY: all index serve deploy clean
