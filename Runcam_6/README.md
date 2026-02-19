
# RunCam Serial Interface (Testing Tool)

This directory contains a C++ command-line tool for testing and controlling RunCam devices over a serial (UART/COM) connection. It is intended for development and diagnostic purposes mainly.

## Features

- Send basic control commands to a RunCam (power(simulation - can not turn on/off), OSD, etc.)
- Write custom text to the OSD
- Simple finite state machine (FSM) for command queueing
- Supports "blind mode" (no response required)

## Usage

1. **Build the Project**

- Open the project in your C++ IDE or use a command-line compiler (e.g., MSVC, MinGW).
- Ensure you are building on **Windows** (uses Win32 serial APIs).

2. **Connect the RunCam**

- Attach the RunCam to your PC via a USB-to-serial adapter.
- Note the COM port number (e.g., `COM3`).

3. **Run the Interface**

- Launch the compiled executable.
- When prompted, enter the COM port **number** (e.g., `3` for `COM3`).

4. **Menu Options**

### Use the keyboard to select commands:

- `[1]` Power Toggle
- `[2]` Start Record - Non Functional (At least outside OSD)
- `[3]` Stop Record - Non Functional (At least outside OSD)
- `[4]` WiFi Button
- `[5]` Change Mode
- ----- OSD and settings -----
- `[6]` OSD Menu OPEN (locks camera)
- `[7]` OSD Menu CLOSE (unlocks camera)
- `[8]` Write Text (requires OSD open)
- `[9]` Get Info
- `[10]` Get Settings
- `[11]` Read Detail
- `[12]` Write Setting
- ----- OSD Navigation -----
- `[13]` OSD ENTER PRESS
- `[14]` OSD ENTER RELEASE
- `[w]` OSD UP
- `[a]` OSD LEFT
- `[s]` OSD DOWN
- `[d]` OSD RIGHT
- `[p]` Toggle protocol for text (0x22/0x23)
- `[x]` Exit

5. **Notes**

- The tool defaults to "blind mode" (commands are sent without waiting for a response).
- For text writing, you will be prompted for X/Y coordinates and the text string.
- OSD must be open to write text.
- This tool is for **testing and development**. Use with caution on production devices.

## File Overview

- `New_RunCam_interface.cpp` — Main interface and FSM logic
- `New_RunCam_registry.h` — Protocol constants and packet templates

## Disclaimer

This tool is provided for development and testing only. Use at your own risk. Ensure you have backups and do not use on critical hardware without proper validation.
