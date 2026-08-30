# Wii U autoboot fork

This is ZestyTS's Wii U autoboot fork of [Hydr8gon/NooDS](https://github.com/Hydr8gon/NooDS).
Use this fork's Releases for an autoboot runtime, not the upstream releases.
The emulator's original authorship and LICENSE are unchanged.

## Using it with UInjectForge

Select the release's `noods.rpx` as the NooDS runtime in UIF. UIF supplies the
game and per-title configuration while building the installable.
The standalone `noods.wuhb` is for SD/homebrew launch.
See README.md for the supported autoboot paths and layout keys.

## Source and build identity

The existing autoboot source is preserved on `codex/noods-known-good-plus`.
This release-preparation branch adds documentation only.
Build with the Wii U devkitPro toolchain and SDL2:
`make wiiu -j2`, preferably from a fresh checkout.
Record source commit, toolchain versions/digest and RPX/WUHB SHA-256 in every release.

The retained RPX hash
`c5a4b7872814ca631d3beba7684a6a21b3f2b3665ab7f6006a04e0aeaa31a002`
predates the latest source commit. Its exact source revision was not recorded.
Do not tag today's source as the verified source of that historical binary.
Keep the retained working binary separate and hardware-test any newly rebuilt
runtime before replacing it. No existing binaries were rebuilt or removed
during repository preparation.

## Reporting problems

Include runtime hash, UIF version, title/game identity, console environment,
and whether launch reached the emulator. Attach
`sd:/uinjectforge/noods/autoboot.log` if present; absence of a log is useful
information but does not identify the cause on its own. Do not attach games,
BIOS files, keys, or saves containing private information.
