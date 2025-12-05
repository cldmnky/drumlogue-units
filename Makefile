# Drumlogue Units - Root Makefile
# Release plumbing for multi-unit repository
#
# Usage:
#   make build UNIT=clouds-revfx           # Build a specific unit
#   make release UNIT=clouds-revfx VERSION=1.0.0-pre  # Prepare release
#   make tag UNIT=clouds-revfx VERSION=1.0.0-pre      # Create git tag
#   make version UNIT=clouds-revfx VERSION=1.0.0      # Update version in header.c
#   make clean UNIT=clouds-revfx           # Clean build artifacts

SHELL := /bin/bash
.PHONY: all build clean release tag version help

# Default target
all: help

# Validate UNIT parameter
check-unit:
ifndef UNIT
	$(error UNIT is required. Usage: make <target> UNIT=<unit-name>)
endif
	@if [ ! -d "drumlogue/$(UNIT)" ]; then \
		echo "Error: Unit directory drumlogue/$(UNIT) does not exist"; \
		exit 1; \
	fi

# Validate VERSION parameter
check-version: check-unit
ifndef VERSION
	$(error VERSION is required. Usage: make <target> UNIT=<unit-name> VERSION=<x.y.z[-suffix]>)
endif
	@if ! echo "$(VERSION)" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9]+)?$$'; then \
		echo "Error: VERSION must be in format x.y.z or x.y.z-suffix (e.g., 1.0.0, 1.0.0-pre, 2.1.3-beta)"; \
		exit 1; \
	fi

# Build a specific unit
build: check-unit
	@echo ">> Building unit: $(UNIT)"
	./build.sh $(UNIT)

# Clean build artifacts for a specific unit
clean: check-unit
	@echo ">> Cleaning unit: $(UNIT)"
	./build.sh $(UNIT) clean

# Update version in unit header.c
# Converts semantic version (x.y.z[-suffix]) to hex format (major<<16 | minor<<8 | patch)
version: check-version
	@echo ">> Updating version for $(UNIT) to $(VERSION)"
	@HEADER_FILE="drumlogue/$(UNIT)/header.c"; \
	BASE_VERSION=$$(echo "$(VERSION)" | sed 's/-.*//'); \
	MAJOR=$$(echo "$$BASE_VERSION" | cut -d. -f1); \
	MINOR=$$(echo "$$BASE_VERSION" | cut -d. -f2); \
	PATCH=$$(echo "$$BASE_VERSION" | cut -d. -f3); \
	HEX_VERSION=$$(printf "0x%02X%02X%02XU" $$MAJOR $$MINOR $$PATCH); \
	COMMENT="v$(VERSION)"; \
	if echo "$(VERSION)" | grep -q '-'; then \
		COMMENT="v$$BASE_VERSION ($(VERSION))"; \
	else \
		COMMENT="v$(VERSION) (major<<16 | minor<<8 | patch)"; \
	fi; \
	if [ ! -f "$$HEADER_FILE" ]; then \
		echo "Error: Header file $$HEADER_FILE not found"; \
		exit 1; \
	fi; \
	sed -i.bak "s/\.version = 0x[0-9A-Fa-f]*U,.*$$/.version = $$HEX_VERSION,                                \/\/ $$COMMENT/" "$$HEADER_FILE" && \
	rm -f "$$HEADER_FILE.bak"; \
	echo ">> Updated $$HEADER_FILE: .version = $$HEX_VERSION  // $$COMMENT"

# Get current version from header.c
get-version: check-unit
	@HEADER_FILE="drumlogue/$(UNIT)/header.c"; \
	HEX=$$(grep '\.version = 0x' "$$HEADER_FILE" | sed 's/.*0x\([0-9A-Fa-f]*\)U.*/\1/'); \
	if [ -z "$$HEX" ]; then \
		echo "Error: Could not parse version from $$HEADER_FILE"; \
		exit 1; \
	fi; \
	MAJOR=$$((16#$${HEX:0:2})); \
	MINOR=$$((16#$${HEX:2:2})); \
	PATCH=$$((16#$${HEX:4:2})); \
	echo "$$MAJOR.$$MINOR.$$PATCH"

# Create git tag for release
tag: check-version
	@TAG_NAME="$(UNIT)/v$(VERSION)"; \
	echo ">> Creating tag: $$TAG_NAME"; \
	if git rev-parse "$$TAG_NAME" >/dev/null 2>&1; then \
		echo "Error: Tag $$TAG_NAME already exists"; \
		exit 1; \
	fi; \
	git tag -a "$$TAG_NAME" -m "Release $(UNIT) v$(VERSION)"
	@echo ">> Tag created. Push with: git push origin $(UNIT)/v$(VERSION)"

# Prepare release (update version, build, and show release info)
release: check-version
	@echo ">> Preparing release: $(UNIT) v$(VERSION)"
	$(MAKE) version UNIT=$(UNIT) VERSION=$(VERSION)
	$(MAKE) build UNIT=$(UNIT)
	@echo ""
	@echo "=============================================="
	@echo "Release preparation complete!"
	@echo "=============================================="
	@echo "Unit:    $(UNIT)"
	@echo "Version: $(VERSION)"
	@echo "Tag:     $(UNIT)/v$(VERSION)"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Review changes: git diff"
	@echo "  2. Commit: git commit -am 'Release $(UNIT) v$(VERSION)'"
	@echo "  3. Tag: make tag UNIT=$(UNIT) VERSION=$(VERSION)"
	@echo "  4. Push: git push && git push origin $(UNIT)/v$(VERSION)"
	@echo ""
	@ARTIFACT=$$(find drumlogue/$(UNIT) -name "*.drmlgunit" 2>/dev/null | head -1); \
	if [ -n "$$ARTIFACT" ]; then \
		echo "Build artifact: $$ARTIFACT"; \
	fi

# List all available units
list-units:
	@echo "Available units:"
	@for dir in drumlogue/*/; do \
		if [ -f "$${dir}header.c" ]; then \
			UNIT_NAME=$$(basename "$$dir"); \
			VERSION=$$($(MAKE) -s get-version UNIT=$$UNIT_NAME 2>/dev/null || echo "unknown"); \
			echo "  - $$UNIT_NAME (v$$VERSION)"; \
		fi; \
	done

# Help target
help:
	@echo "Drumlogue Units - Release Management"
	@echo ""
	@echo "Usage: make <target> UNIT=<unit-name> [VERSION=<x.y.z[-suffix]>]"
	@echo ""
	@echo "Targets:"
	@echo "  build        Build a specific unit"
	@echo "  clean        Clean build artifacts"
	@echo "  version      Update version in unit's header.c"
	@echo "  get-version  Display current version of a unit"
	@echo "  tag          Create git tag for release"
	@echo "  release      Full release preparation (version + build)"
	@echo "  list-units   List all available units"
	@echo "  help         Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make build UNIT=clouds-revfx"
	@echo "  make version UNIT=clouds-revfx VERSION=1.0.0"
	@echo "  make release UNIT=clouds-revfx VERSION=1.0.0-pre"
	@echo "  make tag UNIT=clouds-revfx VERSION=1.0.0-pre"
	@echo "  make list-units"
