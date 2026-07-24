# G6 HE DIY DVT Fix

An experimental, clean-room Zephyr firmware for an nRF54LM20A-based G6 HE
prototype. This repository is a research and recovery project; it is not an
official Keychron release and is not affiliated with Keychron.

Current development version: `0.2.0-dvt1`.

## What this build fixes

The implementation addresses the wired input-path fault behind DVT issue
`T-02`:

- `middle + back` cycles polling rate without reporting middle/back to the
  host;
- `middle + forward` cycles DPI without reporting middle/forward to the host;
- a 25 ms quarantine holds shortcut members until the shortcut can be
  recognised;
- a consumed shortcut clears its host-visible button state, preventing a stuck
  press after the shortcut ends;
- health counters expose shortcut and suppressed-report activity.

The behavior is covered by host unit tests. It has **not** been validated on a
physical mouse running this image. See [DVT_STATUS.md](DVT_STATUS.md) for every
reported issue and its exact status.

## Strict limits and safety boundary

This project is deliberately **not** a drop-in replacement for the production
firmware.

- It produces standalone RRAM/SWD images (`.bin` and `.hex`), not a Keychron
  MCUboot RAM-load `.signed.bin` package.
- It is not signed with Keychron's private Ed25519 key and must never be sent
  to the official Keychron HID/Windows updater.
- The USB identity is intentionally `3434:D687`, not the production
  `3434:D086`, to avoid host descriptor caching and target confusion.
- It only contains the verified wired optical-button/PAW bring-up. It does not
  implement the production 2.4 GHz receiver, BLE, charger, battery, LED,
  Hall/magnetic-switch, or factory pairing stacks.
- Flashing requires a full SWD backup, a uniquely identified nRF54LM20A, and a
  tested recovery procedure on a spare/recoverable unit. No physical device
  has been flashed by this repository.

The original vendor image, signing key, receiver firmware and user data are
not included in this repository.

## Build

Requirements:

- Zephyr 4.4.x workspace with the nRF54LM20DK board definition;
- GNU Arm Embedded Toolchain 15.2 or compatible;
- `west` available in the active Python environment.

```sh
west build \
  -b nrf54lm20dk/nrf54lm20a/cpuapp \
  . \
  -d build-g6-dvt-fix \
  --pristine
```

The output is located in `build-g6-dvt-fix/zephyr/`.

## Tests

```sh
sh tools/run_host_tests.sh
G6_BUILD_DIR=build-g6-dvt-fix/zephyr python3 tools/verify_build_contract.py
```

The host suite validates configuration bounds, CRC frames, DPI/polling stage
cycling, shortcut consumption, and standalone side-button delay behavior.

## Hardware scope

The wired board map in [BOARD_MAP.md](BOARD_MAP.md) contains only pins with
binary-derived evidence. Pins needed for wireless, battery, RGB, physical
mode selection, Hall inputs and the alternate sensor BOM remain intentionally
unconfigured. Extending the firmware requires schematic confirmation and
hardware validation; do not infer unlisted pin functions.

## License

The clean-room source authored in this repository is available under the
[MIT License](LICENSE). Zephyr, its SDK, and any external toolchain remain
subject to their own licenses.
