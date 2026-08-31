# Source build check - 2026-08-30

Source commit tested: b5c6878 (runtime code unchanged from 84c35a7).
Built in a separate temporary clone, not over the retained working binaries.

Toolchain:
wiiuenv/devkitppc:20220907
Digest: sha256:89c103af512364934da0cbd816e8f570a4fea159a436cf6d06a5714473c87310

Command: make wiiu -j2

Compilation passed. New validation-only outputs:
- RPX SHA-256: 055b734ce531ba20b3f280fe13952df589e9e25a7c70e08a5e67a8704de27b85
- WUHB SHA-256: e626e9d6e279e6358bf5133bf4a4b50e391e32e8dcd2d61fa5a68ffdaf823dd2

These differ from the retained historical runtime. This check does not
establish why, reconstruct its unknown source revision, or prove hardware boot.
Do not replace a known-working runtime release based only on this build check.
The GitHub workflow produces build artifacts, not an automatically published
or hardware-approved release.
