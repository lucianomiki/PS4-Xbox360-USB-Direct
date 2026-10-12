# PS4 Xbox 360 USB Direct

## Available USB releases

### v0.7 - DualShock 3 (PS3) and Xbox 360 USB

**[Download v0.7 ELF](https://github.com/lucianomiki/PS4-Xbox360-USB-Direct/releases/download/v0.7-usb/control4free-ps3-xbox360-usb-v0_7.elf)**

PS4 firmware 13.02 with GoldHEN.
This is the preserved v0.7 binary, renamed for clarity.
The original build still includes experimental Bluetooth
components. Wireless DS3 support is not confirmed.

### v0.2.0 - Xbox 360 USB

**[Download v0.2.0 ELF](https://github.com/lucianomiki/PS4-Xbox360-USB-Direct/releases/download/v0.2.0/control4free-usb-v0_2.elf)**

The previous release remains available and unchanged.

## Original v0.2.0 documentation


### 🎮 Use a wired Xbox 360 controller directly on your PS4!

**[⬇ DOWNLOAD READY-TO-USE ELF — PS4 FW 13.02](https://github.com/lucianomiki/PS4-Xbox360-USB-Direct/releases/download/v0.2.0/control4free-usb-v0_2.elf)**

No PC, phone or external adapter required during gameplay.

## Features

- Direct Xbox 360 USB connection to PS4
- Xbox Guide button works as the PS button
- Back = Share / Start = Options
- Native virtual DualShock 4 controller
- Supports local multiplayer
- Automatic USB reconnection
- Keeps the same PS4 user after reconnection
- Manual launch with GoldHEN

## Tested configuration

- PS4 firmware: **13.02**
- Wired Xbox 360 controller
- GoldHEN environment
- Tested with SpiderHeck and Cuphead
- Normal FPS and no duplicated players

Compatibility with other firmware versions and Xbox controller
models has not yet been verified.

## Installation

1. Download `control4free-usb-v0_2.elf` from Releases.
2. Enable FTP on your PS4 through GoldHEN.
3. Upload the ELF to `/data/payloads/`.
4. Connect your wired Xbox 360 controller.
5. Launch the ELF using GoldHEN PayLoader LaunchPad.
6. Select your PS4 user and start playing.

Do not launch another Control4Free instance simultaneously.
Disable conflicting Xbox controller plugins.

## Credits and license

Based on [Control4Free](https://github.com/MoHadiShibli/Control4Free)
by MoHadiShibli.

USB controller research reference:
[Xbox Controller Plugin](https://github.com/crucifix86/xbox_controller_plugin).

This project is a modified adaptation of Control4Free, not
an independently developed replacement.

Distributed under GPL-3.0. See [LICENSE](LICENSE).
Additional notices are in [THIRD_PARTY.md](THIRD_PARTY.md).

Maintained by [lucianomiki](https://github.com/lucianomiki).
