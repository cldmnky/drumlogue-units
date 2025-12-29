# drumlogue-units

Custom DSP units for the **Korg drumlogue** drum machine, bringing professional-grade effects and synthesizers to your groovebox.

[![GitHub Pages](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://cldmnky.github.io/drumlogue-units/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![GitHub Release](https://img.shields.io/github/v/release/cldmnky/drumlogue-units)](https://github.com/cldmnky/drumlogue-units/releases)

**📖 [Visit the Documentation Site](https://cldmnky.github.io/drumlogue-units/)** for detailed unit information, sound demos, and installation guides.

---

## 🎛️ Available Units

### 🌫️ Clouds Reverb FX (v1.2.1)
Lush atmospheric reverb with texture processing, micro-granular effects, pitch shifting, and dual assignable LFOs. Based on Mutable Instruments Clouds.

**Features:** Granular reverb • Pitch shifter • Diffuser • Dual LFOs • 4 texture modes

[Documentation](https://cldmnky.github.io/drumlogue-units/units/clouds-revfx/) • [Download](https://github.com/cldmnky/drumlogue-units/releases/tag/clouds-revfx/v1.2.1)

### 🔔 Elementish Synth (v1.2.0)
Modal synthesis voice combining bow, blow, and strike exciters with versatile resonator models. Based on Mutable Instruments Elements.

**Features:** Physical modeling • Bow/Blow/Strike exciters • Resonator • 6 presets • Comprehensive modulation

[Documentation](https://cldmnky.github.io/drumlogue-units/units/elementish-synth/) • [Download](https://github.com/cldmnky/drumlogue-units/releases/tag/elementish-synth/v1.2.0)

### 🌊 Pepege-Synth (v1.0.0)
2-voice polyphonic PPG wavetable synthesizer with dual oscillators, comprehensive modulation, and authentic 8-bit character.

**Features:** 2-voice polyphony • 16 wavetable banks • 3 interpolation modes • Dual ADSR • LFO • State-variable filter

[Documentation](https://cldmnky.github.io/drumlogue-units/units/pepege-synth/) • [Download](https://github.com/cldmnky/drumlogue-units/releases/tag/pepege-synth/v1.0.0)

### 🪐 Drupiter Synth (v1.1.0)
Jupiter-8 inspired polyphonic/monophonic synthesizer with dual DCOs, multi-mode filter, hard sync, and cross-modulation. Based on Bristol Jupiter-8 emulation.

**Features:** Polyphonic/Unison/Mono modes • Dual DCOs • Hard sync • Cross-modulation • Multi-mode filter • Dual ADSR • 12 presets

[Documentation](https://cldmnky.github.io/drumlogue-units/units/drupiter-synth/) • [Download](https://github.com/cldmnky/drumlogue-units/releases/tag/drupiter-synth/v1.1.0)

---

## 🚀 Quick Start for Users

### Installation

1. **Download** the `.drmlgunit` file from the [releases page](https://github.com/cldmnky/drumlogue-units/releases)
2. **Connect** your drumlogue to your computer via USB (appears as USB drive)
3. **Copy** the file to the `Units/` folder on the drumlogue
4. **Eject** safely and power cycle the drumlogue
5. **Select** the unit from the drumlogue's unit menu

For detailed instructions, see the [Installation Guide](https://cldmnky.github.io/drumlogue-units/installation/).

---

## 🛠️ Development Setup

This repository combines:
- **Mutable Instruments eurorack modules** (`eurorack/`) - High-quality open-source DSP algorithms
- **Korg logue SDK** (`logue-sdk/`) - Official SDK for building drumlogue units
- **Custom build tools** - Containerized build system for consistent compilation

## Quick Start

### Prerequisites

- **Podman or Docker** (for containerized builds)
- **Git** with submodule support
- **GitHub Copilot** (optional, for AI-assisted development)

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

**Using the build script (Recommended):**
```bash
./build.sh <unit-name>        # Build a unit
./build.sh <unit-name> clean  # Clean build
```

**Using the Makefile (for releases):**
```bash
make build UNIT=<unit-name>                    # Build
make version UNIT=<unit-name> VERSION=1.0.0    # Update version
make release UNIT=<unit-name> VERSION=1.0.0    # Full release prep
make tag UNIT=<unit-name> VERSION=1.0.0        # Create git tag
```

**Example:**
```bash
./build.sh drupiter-synth
```

The build produces a `.drmlgunit` file in `drumlogue/<unit-name>/` that can be loaded onto the drumlogue via USB.

## Project Structure

```
drumlogue-units/
├── .github/
│   ├── agents/                         # Custom Copilot agents
│   │   └── drumlogue-dsp-expert.md    # DSP development expert
│   ├── workflows/                      # CI/CD pipelines
│   │   ├── qemu-test.yml              # QEMU ARM testing
│   │   ├── dsp-test.yml               # Desktop DSP tests
│   │   └── pages.yml                  # GitHub Pages deployment
│   ├── instructions/                   # Development guidelines
│   ├── prompts/                        # Reusable prompts
│   └── copilot-instructions.md        # Copilot guidance
├── drumlogue/                          # Custom units
│   ├── clouds-revfx/                  # Clouds reverb effect
│   ├── elementish-synth/              # Elementish synthesizer
│   ├── pepege-synth/                  # PPG wavetable synth
│   ├── drupiter-synth/                # Jupiter-8 synth
│   └── common/                        # Shared DSP utilities
├── eurorack/                          # Mutable Instruments (submodule)
├── logue-sdk/                         # Korg SDK (submodule)
├── test/                              # Test harnesses
│   ├── qemu-arm/                      # QEMU ARM tests
│   └── <unit-name>/                   # Desktop unit tests
├── docs/                              # GitHub Pages site
├── build.sh                           # Main build script
└── Makefile                           # Release management
```

## 🧪 Testing

### Desktop Testing
Test DSP algorithms on your development machine (fast iteration):

```bash
cd test/<unit-name>
make                     # Build test harness
make test                # Run automated tests
./<unit>_test in.wav out.wav
```

### QEMU ARM Testing
Test actual `.drmlgunit` files in ARM emulation (pre-hardware validation):

```bash
cd test/qemu-arm
./test-unit.sh <unit-name>
```

### CI/CD
- **Desktop tests** run on every PR (Ubuntu + macOS)
- **QEMU ARM tests** validate ARM builds for all units
- **Artifacts** available for download (WAV outputs)

See [Testing Guide](test/qemu-arm/TESTING_GUIDE.md) for details.

## 🎨 Creating New Units

### 1. Choose a Template

Copy from `logue-sdk/platform/drumlogue/`:
- **`dummy-synth`** - Synthesizer unit (drum voices, melodic synths)
- **`dummy-delfx`** - Delay effects (echo, chorus, flanger)
- **`dummy-revfx`** - Reverb effects (spatial, ambience)
- **`dummy-masterfx`** - Master effects (final output stage)

### 2. Configure Your Unit

Edit these files:
- **`config.mk`** - Project name, type, source files, includes
- **`header.c`** - Developer ID, unit metadata, parameters (up to 24)
- **`unit.cc`** - DSP implementation (`unit_init`, `unit_render`, etc.)

### 3. Build and Test

```bash
# Build
./build.sh <unit-name>

# Desktop test
cd test/<unit-name> && make test

# QEMU ARM test
cd test/qemu-arm && ./test-unit.sh <unit-name>

# Hardware test
# Copy .drmlgunit to drumlogue via USB
```

### 4. Release

```bash
make release UNIT=<unit-name> VERSION=1.0.0
# Then commit, tag, and push
```

See [Create Unit Prompt](.github/prompts/create-unit.prompt.md) for detailed workflow.

## 🤖 GitHub Copilot Integration

This repository is optimized for GitHub Copilot:

### Custom Agents

- **@drumlogue-dsp-expert** - Expert mode for C/C++ DSP development
  - ARM Cortex-M optimization
  - Real-time audio constraints
  - Logue SDK APIs
  - DSP algorithm implementation

### Usage

```
Ask @drumlogue-dsp-expert to create a delay effect unit with tempo sync
```

### Documentation

- [Copilot Instructions](.github/copilot-instructions.md) - Repository overview
- [C/C++ Guidelines](.github/instructions/cpp.instructions.md) - Coding standards
- [Build System](.github/instructions/build-system.instructions.md) - Build configuration
- [Testing Standards](.github/instructions/testing.instructions.md) - Test harnesses

### Reusable Prompts

- [Create New Unit](.github/prompts/create-unit.prompt.md)
- [Debug Unit](.github/prompts/debug-unit.prompt.md)
- [Port MI DSP](.github/prompts/port-mi-dsp.prompt.md)
- [Release Unit](.github/prompts/release-unit.prompt.md)

## 📚 Resources

### Documentation
- **[GitHub Pages Site](https://cldmnky.github.io/drumlogue-units/)** - User documentation and guides
- **[Installation Guide](https://cldmnky.github.io/drumlogue-units/installation/)** - Setup instructions
- **[Unit Documentation](https://cldmnky.github.io/drumlogue-units/units/)** - Detailed unit references

### External Links
- [Korg logue SDK](https://github.com/korginc/logue-sdk) - Official SDK documentation
- [Mutable Instruments](https://github.com/pichenettes/eurorack) - Original DSP source code
- [Drumlogue Manual](https://www.korg.com/us/support/download/manual/0/894/5325/) - Official hardware manual

### Development
- [QEMU Testing Guide](test/qemu-arm/TESTING_GUIDE.md) - ARM emulation testing
- [Release Process](.github/prompts/release-unit.prompt.md) - Version management
- [DSP Porting Guide](.github/prompts/port-mi-dsp.prompt.md) - Mutable Instruments ports

## 🤝 Contributing

Contributions welcome! Areas of interest:
- **New units** - Port more Mutable Instruments modules or create original DSP
- **Bug fixes** - Improve stability and performance
- **Documentation** - Enhance guides and examples
- **Testing** - Expand test coverage and CI/CD

Please ensure:
- Code follows [C/C++ style guidelines](.github/instructions/cpp.instructions.md)
- DSP algorithms are well-documented
- Builds successfully with `./build.sh`
- Desktop and QEMU tests pass
- Hardware testing when possible

## 📜 License

- **Project code**: MIT License
- **Eurorack DSP**: MIT (STM32) / GPLv3 (AVR) - See individual module licenses
- **logue SDK**: Check [upstream repository](https://github.com/korginc/logue-sdk)
- **Hardware designs**: CC-BY-SA (Mutable Instruments)

See [LICENSE](LICENSE) and individual directories for details.

## 🆘 Support

### For Users
- **Download units**: [Releases page](https://github.com/cldmnky/drumlogue-units/releases)
- **Installation help**: [Installation Guide](https://cldmnky.github.io/drumlogue-units/installation/)
- **Unit documentation**: [Units page](https://cldmnky.github.io/drumlogue-units/units/)
- **Issues**: [GitHub Issues](https://github.com/cldmnky/drumlogue-units/issues)

### For Developers
- **Development guide**: [Copilot Instructions](.github/copilot-instructions.md)
- **Ask questions**: Use `@drumlogue-dsp-expert` Copilot agent
- **Report bugs**: [GitHub Issues](https://github.com/cldmnky/drumlogue-units/issues)
- **Contribute**: See [Contributing](#-contributing) section

---

<div align="center">

**Built with ❤️ using DSP algorithms from [Mutable Instruments](https://mutable-instruments.net/)**

[Documentation](https://cldmnky.github.io/drumlogue-units/) • [Releases](https://github.com/cldmnky/drumlogue-units/releases) • [Issues](https://github.com/cldmnky/drumlogue-units/issues)

</div>
