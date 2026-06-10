# Steam Controller Wrapper
(I'm currently looking for a better name.)

SCWrapper is a Linux application that translates inputs from the 2026 version of the Steam Controller into standard gamepad controls without depending on the Steam Client. This enables the Steam Controller to function in nontraditional environments such as on the MisterFPGA or Raspberry Pi where a Steam client may be unavailable or the default keyboard + mouse control scheme is unsuitable.

### Features:
- Basic Steam Controller support (buttons, triggers, touchpads, and joysticks.)
- Support for the Steam Controller Puck (i.e. handling multiple controllers) --- Note: This is untested, because I only have one controller.
- *Multiple* Puck Support: up to 16 controllers, but can be compiled to handle more. --- Note: Untested for the same reason.
- Rumble Support
- Supports:
  - x86_64 Linux
  - ARMv7L Linux (for the MisterFPGA)
- Static Compilation with Musl

### Planned Features:
- Gyro Support
- Alternative Touchpad Configurations
- Haptics Support (?)

## MisterFPGA / MisterPi Installation Guide

1. Add the following to `/media/fat.downloader.ini`:

```ini
[bnpfeife/scwrapper]
db_url = https://raw.githubusercontent.com/bnpfeife/scwrapper/db/db.json.zip
```

2. Then run `update` or `update_all`
3. Or download [scwrapper.sh](https://raw.githubusercontent.com/bnpfeife/scwrapper/refs/heads/mister/Scripts/scwrapper.sh) and place it in the MisterFPGA's `Scripts` directory.
4. Boot up the MisterFGPA with a keyboard attached, and run the `scwrapper.sh` script.
5. Follow the prompts.
6. Plug in your Steam Controller (or Puck) after the MisterFGPA reboots.
7. Enjoy! (Please report any issues or bugs you encounter!)

## Linux Installation Guide

1. Download the [latest](https://github.com/bnpfeife/scwrapper/releases/tag/latest) scwrapper binary from Releases.
2. Run `scwrapper`
3. Plug in your Steam Controller (or Puck).
4. Enjoy! (Please report any issues or bugs you encounter!)

## Build Guide
### For Desktop Linux:

#### Local Development
Dependencies:
- gcc
- make
- cmake
- glibc (development libraries)
- linux-headers (development libraries)

Run this command within the project directory:
```
$ make
```

#### Release Build
Dependencies:
- docker
- make

Run this command within the project directory:
```
$ make scwrapper.x86_64
```

### For MisterFPGA:
Dependencies:
- docker
- make

Run this command within the project directory:
```
$ make scwrapper.arm7vl
```

## Contributing
Please note that this project is in a very early state, so large architectural changes may occur.  Feel free to create an issue or submit a pull-request if you encounter a bug or wish to develop / suggest a feature!
