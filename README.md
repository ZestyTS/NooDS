# NooDS
A (hopefully!) speedy DS emulator.

### Overview
The goal of NooDS is to be a fast and portable DS and GBA emulator. It features accurate software rendering with
upscaling, and can take advantage of multiple cores for better performance. I started working on NooDS for fun and as a
learning experience, and I'd like to keep it that way. If people are interested and get use out of it, that's a bonus!

### Downloads
NooDS is available for Windows, macOS, Linux, Android, Switch, Wii U, and Vita. The latest builds are automatically
provided via GitHub Actions, and can be downloaded from the [releases page](https://github.com/Hydr8gon/NooDS/releases).

### Usage
NooDS should be able to run most things without any setup. DS BIOS and firmware files must be provided to boot the
system menu, and can be dumped from a DS with [DSBF Dumper](https://archive.org/details/dsbf-dumper). The firmware must
come from an original DS; DSi and 3DS dumps don't contain any boot code. A GBA BIOS file can be optionally provided, and
can be dumped from many systems with [this dumper](https://github.com/mgba-emu/bios-dump). File paths can be configured
in the settings menu. Save types are automatically detected, but this may not always be accurate. If you run something
and it has issues with saving, the save type can be overriden in the file menu.

### Wii U Autoboot
The Wii U build can read an optional `autoboot.txt` from the installed title content or SD card and load a ROM before
opening the file browser. Supported locations are:

* `fs:/vol/content/autoboot.txt`
* `fs:/vol/content/noods/autoboot.txt`
* `sd:/wiiu/apps/noods/autoboot.txt`
* `sd:/noods/autoboot.txt`
* `sd:/uinjectforge/noods/autoboot.txt`

The file can contain either a bare ROM path or key/value lines:

```ini
rom=fs:/vol/content/games/game.nds
fallback=filebrowser
show_error=1
log=1
screenArrangement=2
screenSizing=0
screenGap=0
screenRotation=0
```

Optional layout keys use the same values as NooDS settings: `screenPosition` 0-4, `screenRotation` 0-1,
`screenArrangement` 0-3, `screenSizing` 0-2, `screenGap` 0-3, `aspectRatio` 0-3, and `integerScale` 0-1.

Autoboot diagnostics are written to `sd:/uinjectforge/noods/autoboot.log` when the Wii U runtime reaches startup.

### Contributing
This is a personal project, and I've decided to not review or accept pull requests for it. If you want to help, you can
test things and report issues or provide feedback. If you can afford it, you can also donate to motivate me and allow me
to spend more time on things like this. Nothing is mandatory, and I appreciate any interest in my projects, even if
you're just a user!

### Building
**Windows:** Install [MSYS2](https://www.msys2.org) and run the command
`pacman -Syu mingw-w64-x86_64-{gcc,pkg-config,wxwidgets-msw,portaudio,jbigkit} make` to get dependencies. Navigate to the
project root directory and run `make -j$(nproc)` to start building.

**macOS/Linux:** On the target system, install [wxWidgets](https://www.wxwidgets.org) and
[PortAudio](https://www.portaudio.com). This can be done with the [Homebrew](https://brew.sh) package manager on macOS,
or a built-in package manager on Linux. Run `make -j$(nproc)` in the project root directory to start building.

**Android:** Install [Android Studio](https://developer.android.com/studio) or the command line tools. Run
`./gradlew assembleDebug` in the project root directory to start building; dependencies will be installed as needed.

**Switch:** Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and its `switch-dev` package. Run
`make switch -j$(nproc)` in the project root directory to start building.

**Wii U:** Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and its `wiiu-dev` package. Run
`make wiiu -j$(nproc)` in the project root directory to start building.

**Vita:** Install [Vita SDK](https://vitasdk.org) and run `make vita -j$(nproc)` in the project root directory to
start building.

### Hardware References
* [GBATEK](https://problemkaputt.de/gbatek.htm) - The main information source for all things DS and GBA
* [GBATEK Addendum](https://melonds.kuribo64.net/board/thread.php?id=13) - A thread that aims to fill the gaps in GBATEK
* [melonDS Blog](https://melonds.kuribo64.net) - Contains posts that detail various 3D quirks
* [DraStic BIOS](https://drive.google.com/file/d/1dl6xgOXc892r43RzkIJKI6nikYIipzoN/view) - Reference for the DS HLE BIOS
functions
* [Cult of GBA BIOS](https://github.com/Cult-of-GBA/BIOS) - Reference for the GBA HLE BIOS functions

### Other Links
* [Hydra's Lair](https://hydr8gon.github.io) - Blog where I may or may not write about things
* [Discord Server](https://discord.gg/JbNz7y4) - A place to chat about my projects and stuff
