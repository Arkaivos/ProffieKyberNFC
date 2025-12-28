# ProffieKyberNFC

**ProffieKyberNFC** is a dual-part project designed to bring NFC-based Kyber crystals to a custom lightsaber build.

The system allows physical Kyber crystals, embedded with NFC tags, to define both the **blade color**, the **active preset**, and an optional **attunement owner** on a Proffieboard-based lightsaber.

---

## Project Overview

This repository contains two main components:

### 1. NFC Writer (Python)

A Python utility used to **encode NFC tags** embedded in Kyber crystals.

Each tag stores:
- RGB color information
- The name of the preset to be activated on the Proffieboard
- A flair field called **“attuned to”**, used to store the name of the crystal’s owner

---

### 2. ProffieBoard Prop (Kyber NFC Prop)

A custom **ProffieOS prop** that enables NFC-based crystal detection using a PN532 module over I²C.

When a Kyber crystal is inserted:
- The NFC tag is read
- The blade color is updated dynamically
- The corresponding preset is selected automatically
- The crystal’s *attunement* data can be read for identification or display
- An optional crystal chamber LED effect is activated

The system is designed to work while the saber is **powered off**, allowing crystal swaps without igniting the blade.

---

## Features

- NFC-based Kyber crystal identification
- Dynamic blade color assignment
- Automatic preset selection by name
- Crystal attunement field (“attuned to”)
- Crystal chamber LED effects
- Persistent preset storage (SD card)
- Designed for ProffieBoard + PN532 (I²C)

---

## Hardware Requirements

- ProffieBoard
- PN532 NFC module (I²C mode)
- NFC tags compatible with NTAG2xx
- Addressable LED blade
- Optional crystal chamber LED

---

## Software Requirements

- ProffieOS
- Python 3.x (for the NFC writer)
- Required Python NFC libraries (depending on your NFC reader)

---

## Usage

### Writing a Kyber Crystal

1. Connect your NFC writer to your computer
2. Run the Python script
3. Enter:
   - RGB color
   - Preset name
   - Attuned-to (owner) name
4. Write the data to the NFC tag embedded in the crystal

### Using the Crystal in the Saber

1. Power off the lightsaber
2. Insert the Kyber crystal
3. The ProffieBoard reads the NFC tag
4. Blade color and preset are updated automatically
5. Ignite the saber

---

## Notes

- Preset names must match exactly those defined in your ProffieOS configuration
- NFC reading is intentionally disabled while the blade is on
- Crystal chamber LEDs are time-limited to avoid overheating and battery drain
- The *attuned to* field is intended as an identity / flair element and does not affect saber behavior by default

---

## Related Article

A detailed explanation of the project, design decisions, and build process is available on my blog:

https://www.hexadian.com/arkaivos/?p=505

---

## License

This project is shared for personal and educational use.  
