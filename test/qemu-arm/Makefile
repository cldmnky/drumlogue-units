# Makefile that automatically uses Podman for ARM cross-compilation on macOS
# This simplifies the workflow - just use 'make' and it handles the details

# Detect if we're on macOS
UNAME_S := $(shell uname -s)

# On macOS, delegate to Podman-based build
ifeq ($(UNAME_S),Darwin)
	MAKEFILE_TO_USE := Makefile.podman
else
	# On Linux, use native ARM cross-compilation
	MAKEFILE_TO_USE := Makefile.linux
endif

# Define the targets to delegate
.PHONY: all help install-podman check-podman podman-build podman-test test-unit podman-shell generate-signals performance-test clean status setup which-makefile

# Delegate all targets to the appropriate Makefile
all:
	@echo "Detected platform: $(UNAME_S)"
	@echo "Using: $(MAKEFILE_TO_USE)"
	@echo ""
	$(MAKE) -f $(MAKEFILE_TO_USE) all

# Delegate common targets
help install-podman check-podman podman-build podman-test test-unit podman-shell generate-signals performance-test clean status:
	$(MAKE) -f $(MAKEFILE_TO_USE) $@

# Quick setup for new users (macOS)
setup:
ifeq ($(UNAME_S),Darwin)
	@echo "Setting up QEMU ARM testing with Podman on macOS..."
	$(MAKE) -f $(MAKEFILE_TO_USE) install-podman
	@echo ""
	@echo "✓ Setup complete!"
	@echo ""
	@echo "Try:"
	@echo "  make test-unit UNIT=clouds-revfx"
	@echo "  make podman-test"
else
	@echo "Setting up QEMU ARM testing on Linux..."
	$(MAKE) -f $(MAKEFILE_TO_USE) install-deps
	@echo ""
	@echo "✓ Setup complete!"
	@echo ""
	@echo "Try:"
	@echo "  make all"
	@echo "  make test"
endif

# Show which Makefile is being used
which-makefile:
	@echo "Platform: $(UNAME_S)"
	@echo "Using: $(MAKEFILE_TO_USE)"