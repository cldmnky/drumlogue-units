---
layout: default
title: Installation Guide
permalink: /installation/
---

# Installation Guide

Complete guide for installing drumlogue units on your Korg drumlogue.

---

## Requirements

- **Korg drumlogue** (firmware 1.0.0 or later recommended)
- **USB cable** (Type-A to Type-C)
- **Computer** (Windows, macOS, or Linux)
- **Unit files** (`.drmlgunit` format)

---

## Download Units

1. Visit the [GitHub Releases](https://github.com/cldmnky/drumlogue-units/releases) page
2. Download the `.drmlgunit` files for the units you want to install
3. Save them to a location on your computer

---

## Step-by-Step Installation

### 1. Connect Your drumlogue

1. Power on your drumlogue
2. Connect it to your computer via USB cable
3. The drumlogue will appear as a USB mass storage device

### 2. Navigate to the Units Folder

Open the drumlogue drive and navigate to the appropriate folder based on the unit type:

| Unit Type | Folder Location |
|-----------|-----------------|
| **Synthesizers** | `Units/Synths/` |
| **Delay Effects** | `Units/DelayFXs/` |
| **Reverb Effects** | `Units/ReverbFXs/` |
| **Master Effects** | `Units/MasterFXs/` |

### 3. Copy the Unit Files

- **Clouds Reverb FX** (`clouds_revfx.drmlgunit`) → `Units/ReverbFXs/`
- **Elementish Synth** (`elementish_synth.drmlgunit`) → `Units/Synths/`

### 4. Safely Eject

**Important**: Always safely eject the drumlogue before disconnecting:

- **Windows**: Right-click on the drive and select "Eject"
- **macOS**: Drag the drive to the Trash or press ⏏ next to the drive in Finder
- **Linux**: Use your file manager's eject option or `umount` command

### 5. Load the Units

After safely ejecting the drumlogue, press the **(PLAYBACK)/(STOP)** button on the drumlogue to load the new units.

**Note**: No power cycling is required - the units will be loaded automatically when you press the PLAYBACK/STOP button.

---

## Accessing Installed Units

### Synthesizer Units

1. Go to the **SYNTH** page on your drumlogue
2. Press the encoder or navigate to select the synth engine
3. Scroll through the list to find your installed synth
4. The unit name (e.g., "Elementish") will appear in the selection

### Effect Units

1. Go to the effects section (**DEL FX**, **REV FX**, or **MASTER FX**)
2. Select the effect slot
3. Scroll through available effects to find your installed unit
4. The unit name (e.g., "Clds Reverb") will appear in the selection

---

## Troubleshooting

### Unit Doesn't Appear

1. **Check the folder**: Ensure the `.drmlgunit` file is in the correct `Units/` subfolder
2. **Load units**: Press the **(PLAYBACK)/(STOP)** button on the drumlogue to load units
3. **File integrity**: Re-download the unit file if it may be corrupted
4. **Firmware**: Update your drumlogue firmware to the latest version

### USB Connection Issues

1. Try a different USB cable
2. Try a different USB port on your computer
3. Avoid using USB hubs - connect directly to your computer
4. On Windows, check Device Manager for driver issues

### Unit Crashes or Causes Issues

1. Remove the problematic `.drmlgunit` file
2. Press the **(PLAYBACK)/(STOP)** button to reload units
3. Report the issue on the [GitHub Issues](https://github.com/cldmnky/drumlogue-units/issues) page with:
   - Drumlogue firmware version
   - Unit version
   - Steps to reproduce the issue

---

## Updating Units

To update a unit to a newer version:

1. Download the new `.drmlgunit` file
2. Connect your drumlogue via USB
3. Copy the new file to the same location, **replacing** the old file
4. Safely eject and press the **(PLAYBACK)/(STOP)** button to load the updated unit

**Note**: For synth units with preset support, updating will reset to factory presets. Reverb and delay effect units do not support presets due to drumlogue hardware limitations.

---

## Removing Units

To remove a unit:

1. Connect your drumlogue via USB
2. Navigate to the appropriate `Units/` subfolder
3. Delete the `.drmlgunit` file
4. Safely eject and press the **(PLAYBACK)/(STOP)** button to reload units

---

## Unit Limits

The drumlogue has limits on how many user units can be loaded:

| Unit Type | Maximum |
|-----------|---------|
| Synth | 16 |
| Delay FX | 16 |
| Reverb FX | 16 |
| Master FX | 16 |

If you reach the limit, remove some units to install new ones.

---

## Sample Files

Some units (like synths) may use sample files for additional sounds or drum kits.

### Sample File Naming Rules

Sample files must follow the pattern: `XXX_sample_name.wav`

- **XXX**: Three digits (001-128) indicating the sample index
- **sample_name**: Only alphanumeric characters and these special characters: `-_!?#$%&'()*+,:;<=>@`
- **Note**: Filenames only support single-byte ASCII encodings
- **Invalid files**: Files not following these rules will be deleted from the drumlogue

### Supported Sample Formats

The drumlogue natively uses WAV files encoded in **32-bit float** at **48kHz** sampling rate.

**Supported conversions:**
- 16-bit, 24-bit, 32-bit PCM
- Other common sampling rates (automatically converted on startup)
- **Note**: Converting samples increases startup time

### Installing Sample Files

1. Connect your drumlogue via USB
2. Copy sample files to the `Samples/` folder
3. Follow the same naming convention as above
4. Safely eject and press the **(PLAYBACK)/(STOP)** button to load the samples

---

## Building from Source

If you want to build units from source code:

### Prerequisites

- Docker (for the build environment)
- Git with submodule support

### Build Steps

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/cldmnky/drumlogue-units.git
cd drumlogue-units

# Build using the build script
./build.sh clouds-revfx

# Or use the Docker build environment directly
logue-sdk/docker/run_interactive.sh
# Inside container:
build drumlogue/clouds-revfx
```

The built `.drmlgunit` file will be in the project's directory (e.g., `drumlogue/clouds-revfx/`).

---

## Support

- **GitHub Issues**: [Report bugs or request features](https://github.com/cldmnky/drumlogue-units/issues)
- **GitHub Discussions**: [Ask questions and share tips](https://github.com/cldmnky/drumlogue-units/discussions)
