<!-- SVENKUNKKA-README-NAV:BEGIN -->
[中文项目说明](#project-overview-zh) · [English overview](#project-overview-en) · [维护者 X / Maintainer on X](https://x.com/Svenu1s5)
<!-- SVENKUNKKA-README-NAV:END -->

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


---

<!-- SVENKUNKKA-PROJECT-INFO:BEGIN -->

<a id="project-overview-zh"></a>
## 中文项目说明

面向 nRF54LM20A G6 HE 原型的实验性洁净室 Zephyr 固件，围绕 DVT T-02 有线输入路径增加快捷键隔离、组合键消费和状态计数，避免组合键成员泄露为普通按键报告。

**使用入口：** 在匹配的 Zephyr 4.4.x 工作区按 README 的 `west build` 命令构建；使用 `sh tools/run_host_tests.sh` 运行主机测试，并查看 [DVT_STATUS.md](DVT_STATUS.md)。

**范围说明：** 主机测试不代表实机通过。输出是独立 RRAM/SWD 映像，不是官方签名升级包，不能送入官方更新器；未包含量产无线、充电、电池、磁轴或工厂配对栈。

<a id="project-overview-en"></a>
## English project overview

Experimental clean-room Zephyr firmware for an nRF54LM20A G6 HE prototype, addressing the DVT T-02 wired input path with shortcut quarantine, consumption and counters to prevent shortcut members from leaking into ordinary button reports.

**Getting started:** Build in a matching Zephyr 4.4.x workspace using the README’s `west build` command. Run host tests with `sh tools/run_host_tests.sh` and consult [DVT_STATUS.md](DVT_STATUS.md).

**Scope:** Host tests are not hardware validation. Outputs are standalone RRAM/SWD images, not official signed update packages, and must not be sent to the official updater. Production wireless, charging, battery, magnetic-switch and factory-pairing stacks are not included.

**仓库维护者 / Repository maintainer:** [SvenKunkka](https://github.com/SvenKunkka) · [X / Twitter](https://x.com/Svenu1s5)

<!-- SVENKUNKKA-PROJECT-INFO:END -->
