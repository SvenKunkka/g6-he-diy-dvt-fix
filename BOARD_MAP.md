# G6 HE board map (binary-derived)

Source image: `G6HE_v1.0.0+5_20260722.signed.bin`

The addresses below were recovered from the production image's Zephyr device
objects and GPIO tables. Only pins marked `confirmed` are driven by the
replacement firmware.

| Function | nRF54LM20A pin | Electrical/config evidence | Status |
| --- | --- | --- | --- |
| Sensor SPI SCK | P2.1 | SPIM pinctrl `0x04000041` | confirmed |
| Sensor SPI MOSI | P2.2 | SPIM pinctrl `0x05000042` | confirmed |
| Sensor SPI MISO | P2.4 | SPIM pinctrl `0x06000044` | confirmed |
| Sensor SPI CS | P2.5 | GPIO2 pin 5, active low | confirmed |
| Sensor reset/enable | P2.3 | high, 1 ms, low, 100 us, high, 10 ms | confirmed |
| Wheel encoder A/B | P0.1 / P0.0 | adjacent active-low input specs | confirmed |
| Button 1 (left) | P1.9 | logical button index 0 | confirmed |
| Button 2 (right) | P1.17 | logical button index 1 | confirmed |
| Button 3 (wheel) | P1.28 | logical button index 2 | confirmed |
| Button 4 (back) | P1.30 | logical button index 3 | confirmed |
| Button 5 (forward) | P1.29 | logical button index 4 | confirmed |

Sensor bus configuration is 16 MHz, 8-bit, MSB-first, SPI mode 3. The current
production sensor identifies itself by register 0 as `0x6e`; the alternate BOM
profile uses `0x53`.

Pins P1.2, P1.3, P1.6, P1.14, P1.23, P0.2, and P0.4 remain unassigned. They
are not configured or driven until the mode-switch, RGB, battery, optical
switch, and Hall-sensor roles are separated with equivalent evidence.

