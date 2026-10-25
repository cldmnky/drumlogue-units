# Web emulator build: compiles a drumlogue unit's sources to WebAssembly.
#
#   make                       # builds www/units/pepege-synth.wasm
#   make UNIT=clouds-revfx     # any unit under drumlogue/
#   make serve                 # serves www/ on http://localhost:8000
#   make clean
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

# The unit's own config tells us its sources and defines. USE_NEON has to go:
# there is no NEON in WebAssembly and every unit has scalar fallbacks.
include $(UNIT_DIR)/config.mk
WASM_DEFS := $(filter-out -DUSE_NEON,$(UDEFS))

INCLUDES := -I$(UNIT_DIR) -I$(SDK_COMMON) $(addprefix -I$(UNIT_DIR)/,$(UINCDIR))
SOURCES := $(addprefix $(UNIT_DIR)/,$(CXXSRC) $(CSRC)) src/unit_shim.c

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

$(OUT): $(SOURCES) src/unit_shim.c
	@mkdir -p $(OUT_DIR)
	$(EMXX) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@
	@$(MAKE) --no-print-directory index

index:
	@cd $(OUT_DIR) && ls *.wasm 2>/dev/null | sed 's/\.wasm$$//' \
	  | python3 -c 'import json,sys; print(json.dumps([l.strip() for l in sys.stdin if l.strip()]))' \
	  > index.json
	@echo "units: $$(cat $(OUT_DIR)/index.json 2>/dev/null)"

serve:
	@echo "http://localhost:8000/"
	@python3 -m http.server 8000 --directory www

# Serve the built site from a ConfigMap on OpenShift; see deploy/README.md.
NAMESPACE ?= pi-pocket
deploy:
	kubectl create configmap drumlogue-web -n $(NAMESPACE) \
	  --from-file=index.html=www/index.html \
	  --from-file=app.js=www/app.js \
	  --from-file=worklet.js=www/worklet.js \
	  --from-file=styles.css=www/styles.css \
	  --from-file=units-index.json=www/units/index.json \
	  --from-file=unit-wasm=www/units/$(UNIT).wasm \
	  --dry-run=client -o yaml | kubectl apply -f -
	kubectl apply -f ../deploy/drumlogue-web.yaml -n $(NAMESPACE)
	kubectl rollout restart deployment/drumlogue-web -n $(NAMESPACE)
	kubectl get route drumlogue-web -n $(NAMESPACE) \
	  -o jsonpath='https://{.spec.host}{"\n"}'

clean:
	rm -rf $(OUT_DIR)

.PHONY: all index serve deploy clean
