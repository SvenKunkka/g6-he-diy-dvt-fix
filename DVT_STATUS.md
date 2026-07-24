# DVT issue status

The table preserves the DVT findings supplied by the test team. `Fixed` means
implemented in this source and covered by a host test; it does **not** mean
validated on a physical mouse. `Unresolved` means no production claim is made.

| ID | Reported issue | Status in `0.2.0-dvt1` | Reason / next requirement |
|---|---|---|---|
| T-01 | Wired and BLE version differ | Unresolved | Production BLE stack is unavailable. |
| T-02 | Wired middle+side shortcut also sends side button | Fixed in source; host-tested | `g6_input_guard` consumes the chord and issues a neutral release. |
| T-03 | 2.4G DPI/polling change triggers all controls | Unresolved | Needs production RF session/report queue implementation. |
| T-04 | Optical buttons fail at 2K/4K/8K | Unresolved | Needs real-time scan/USB/RF measurements on hardware. |
| T-05 | Optical 8K unstable / 2.4G measures ~7600 Hz | Unresolved | Needs production scheduler and RF timing source. |
| T-06 | Wired magnetic 4K/8K measures ~2700 Hz | Unresolved | Hall/ADC pin map and switch driver are intentionally disabled. |
| T-07 | Charging in 2.4G limits 4K/8K to ~2600 Hz | Unresolved | Charger and power policies are not present. |
| T-08 | Receiver replug/reconnect causes ghost input | Unresolved | Needs RF epoch and reconnect implementation. |
| T-09 | Entering 2.4G creates ghost input/stuck left | Unresolved | The generic guard exists, but there is no 2.4G mode state machine here. |
| T-10 | Pairing broadcast is ~2 s instead of 1 s | Unresolved | Pairing scheduler is absent. |
| T-11 | 2.4G reconnect is ~10 s instead of 3 s | Unresolved | Requires receiver protocol and RF measurements. |
| T-12 | BLE reconnect failure cannot wake | Unresolved | BLE sleep/wake implementation is absent. |
| T-13 | Reset does not clear 2.4G pairing | Unresolved | Requires production persistent settings schema. |
| T-14 | Fast 2.4G/BLE switch has abnormal LED | Unresolved | LED/mode controller is absent. |
| T-15 | BLE-mode LED feedback is slow | Unresolved | LED/mode controller is absent. |
| T-16 | Two paired receivers do not reconnect correctly | Unresolved | Requires multi-receiver bond storage and RF protocol. |
| T-17 | 2.4G runtime drops to ~24 Hz | Unresolved | Needs RF/transport heartbeat telemetry and recovery. |
| T-18 | Battery <=3.5 V often fails to boot | Unresolved | Needs power tree, UVLO and boot-sequencing validation. |
| T-19 | USB-only, no-battery boot often fails | Unresolved | Needs VBUS/battery power arbitration design. |
| T-20 | Standby current ~3.2 mA | Unresolved | Needs complete peripheral power-domain control and measurement rig. |

## Why only T-02 is marked fixed

The public source has a verified wired input path and a host-testable shortcut
state machine. It does not contain the proprietary wireless, BLE, receiver or
power code needed to test or repair the remaining reports. Marking those
issues fixed without a board-accurate implementation and physical regression
evidence would be misleading.
