# drumlogue-units

Development repository for creating custom DSP units for the Korg drumlogue drum machine.

## Overview

This repository combines:
- **Mutable Instruments eurorack modules** (`eurorack/`) - High-quality open-source DSP algorithms
- **Korg logue SDK** (`logue-sdk/`) - Official SDK for building drumlogue units

## Quick Start

### Prerequisites

- Docker (for the build environment)
- Git with submodule support
- GitHub Copilot (recommended for AI-assisted development)

### Setup

1. Clone the repository with submodules:
   ```bash
   git clone --recursive https://github.com/cldmnky/drumlogue-units.git
   cd drumlogue-units
   ```

2. Or initialize submodules in an existing clone:
   ```bash
   git submodule update --init --recursive
   ```

### Building Units

This project uses a Docker-based build system to ensure consistent compilation:

**Interactive Development (Recommended):**
```bash
logue-sdk/docker/run_interactive.sh
# Inside the container:
build drumlogue/dummy-synth
build -l --drumlogue  # List all available projects
```

**CI-Style Build:**
```bash
./logue-sdk/docker/run_cmd.sh build drumlogue/dummy-synth
```

**Clean Build:**
```bash
build --clean drumlogue/dummy-synth
```

The build produces a `.drmlgunit` file that can be loaded onto the drumlogue via USB.

## Project Structure

```
.
├── .github/
│   ├── agents/                    # Custom Copilot agents
│   │   └── drumlogue-dsp-expert.md
│   ├── workflows/                 # GitHub Actions
│   │   └── copilot-setup-steps.yml
│   └── copilot-instructions.md    # Repository-specific Copilot guidance
├── eurorack/                      # Mutable Instruments modules (submodule)
├── logue-sdk/                     # Korg logue SDK (submodule)
└── README.md
```

## Creating New Units

1. Copy a template from `logue-sdk/platform/drumlogue/`:
   - `dummy-synth` - Synthesizer unit
   - `dummy-delfx` - Delay effect
   - `dummy-revfx` - Reverb effect
   - `dummy-masterfx` - Master effect

2. Configure your unit:
   - Edit `config.mk` - Project name, type, source files
   - Edit `header.c` - Developer ID, unit metadata, parameters
   - Edit `unit.cc` - DSP implementation

3. Build and test using Docker scripts

## GitHub Copilot Integration

This repository is configured with GitHub Copilot custom agents:

- **@drumlogue-dsp-expert** - Expert in C/C++ DSP development for ARM-based drumlogue units
  - Understands logue SDK APIs
  - Knows DSP algorithms and optimization techniques
  - Familiar with ARM Cortex-M constraints
  - Can guide unit development from scratch

See `.github/copilot-instructions.md` for detailed guidance on building drumlogue units.

## Development Workflow

1. **Plan your unit** - Choose unit type (synth, effect) and desired DSP functionality
2. **Copy template** - Start from an appropriate dummy template
3. **Configure** - Set metadata, parameters, and build configuration
4. **Implement DSP** - Write audio processing code in `unit.cc`
5. **Build** - Use Docker build scripts
6. **Test on hardware** - Copy `.drmlgunit` to drumlogue via USB
7. **Iterate** - Refine parameters and DSP based on testing

## Resources

- [Korg logue SDK Documentation](https://github.com/korginc/logue-sdk)
- [Mutable Instruments](https://github.com/pichenettes/eurorack)
- [GitHub Copilot Best Practices](https://gh.io/copilot-coding-agent-tips)

## Contributing

Contributions welcome! Please ensure:
- Code follows existing style conventions
- DSP algorithms are well-documented
- Builds successfully with Docker toolchain
- Testing on hardware when possible

## License

See individual directories for license information:
- Eurorack code: GPLv3 (AVR) / MIT (STM32)
- logue SDK: Check upstream repository

## Support

For questions and issues:
- Open an issue in this repository
- Use GitHub Copilot with the @drumlogue-dsp-expert agent
- Reference `.github/copilot-instructions.md` for development guidance
