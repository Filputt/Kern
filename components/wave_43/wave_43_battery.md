# Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Battery Monitoring

Reference document for the battery monitoring implementation in
`components/wave_43/pmic.c`.

## Hardware

- **Board:** Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3
- **Charger:** ETA6098 (hardware-managed charging, no I2C interface)
- **Battery connector:** 2-pin JST-style Li-ion/Li-Po (single cell, 3.0–4.2 V)

## Voltage divider

The battery positive rail is routed through a resistive divider before reaching
the ADC pin:

| Resistor | Value | Role |
|----------|-------|------|
| R12      | 200 kΩ | High-side (to BAT+) |
| R15      | 100 kΩ | Low-side (to GND)   |

Divider ratio:

```
Vadc = Vbat * (R15 / (R12 + R15))
Vadc = Vbat * (100k / (200k + 100k))
Vadc = Vbat / 3
```

Therefore:

```
Vbat = Vadc * 3.0
```

## ADC pin

> **Important:** The divider output (`BAT_ADC` net) is connected to **GPIO20**,
> which maps to **ADC1 channel 4** on the ESP32-P4. The original implementation
> used GPIO22, which is also a valid ADC1 pin but is **not** wired to the
> battery divider on this board. This was confirmed by scanning all eight
> ADC1-capable pins (GPIO16–GPIO23) and observing that only GPIO20 reported a
> stable, plausible divider voltage.

| Parameter        | Value                       |
|------------------|-----------------------------|
| ADC GPIO         | GPIO20                      |
| ADC unit         | ADC_UNIT_1                  |
| ADC channel      | ADC1 channel 4              |
| Attenuation      | ADC_ATTEN_DB_12             |
| Resolution       | 12-bit (ADC_BITWIDTH_12)    |
| Max raw value    | 4095                        |

ESP32-P4 ADC1 pin map for reference:

| GPIO | ADC1 channel |
|------|--------------|
| 16   | 0            |
| 17   | 1            |
| 18   | 2            |
| 19   | 3            |
| **20** | **4**      |
| 21   | 5            |
| 22   | 6            |
| 23   | 7            |

## ADC calibration

The ESP32-P4 ADC is inaccurate without calibration. The implementation uses the
native Espressif curve-fitting calibration scheme:

```c
adc_cali_create_scheme_curve_fitting(...)
adc_cali_raw_to_voltage(...)
```

Calibration converts raw ADC ticks into millivolts at the ADC pin, compensating
for chip-to-chip variance and non-linearity.

## Averaging

To smooth voltage ripples caused by the LCD backlight PWM and Wi-Fi transmit
bursts, each battery reading averages **32 ADC samples**. The samples are
individually calibrated to millivolts before averaging.

## Battery percentage

The driver exposes percentage through `bsp_pmic_get_battery_percent()`:

| Threshold | Meaning |
|-----------|---------|
| ≤ 3000 mV | 0 % (empty) |
| ≥ 4200 mV | 100 % (full) |
| 3001–4199 mV | Linearly scaled |

This is a simple voltage-based approximation. Li-ion discharge is non-linear,
so the percentage is accurate near full/empty but less precise in the middle of
the range. It is sufficient for a UI battery icon.

## Software interface

The implementation conforms to `components/bsp_common/include/bsp/pmic.h`:

```c
esp_err_t bsp_pmic_init(void);
bool      bsp_pmic_is_available(void);
esp_err_t bsp_pmic_get_battery_mv(uint16_t *mv);
esp_err_t bsp_pmic_get_battery_percent(uint8_t *pct);
esp_err_t bsp_pmic_get_charge_status(bsp_pmic_chg_t *status);
bool      bsp_pmic_is_vbus_present(void);
```

The UI driver (`main/ui/battery.c`) polls `bsp_pmic_get_battery_percent()` every
30 seconds and selects the appropriate LVGL battery symbol.

## Charge / VBUS detection

The ETA6098 charger does not expose a digital charge-status or VBUS-sense pin
that is routed to a known GPIO on this board variant. Consequently:

- `bsp_pmic_get_charge_status()` always returns `BSP_PMIC_CHG_DISCHARGING`.
- `bsp_pmic_is_vbus_present()` always returns `false`.
- The UI will show a battery icon but no charging bolt overlay.

If a future board revision routes `CHG_STAT` or `VBUS_SENSE` to a GPIO, update
`CHG_STAT_GPIO` and `VBUS_SENSE_GPIO` in `pmic.c`.

## Build / flash

```bash
just build wave_43
just flash wave_43
```

## Verification

With a fully charged battery on USB power, expect:

```text
Vadc ≈ 1380–1400 mV
Vbat ≈ 4140–4200 mV
percentage ≈ 95–100 %
```

## Known limitations / future improvements

1. **Non-linear SOC:** The current linear 3000–4200 mV mapping is approximate.
   A proper Li-ion discharge curve would improve mid-range accuracy.
2. **No charge indicator:** Without `CHG_STAT`/`VBUS_SENSE` GPIOs, the UI cannot
   distinguish charging from discharging.
3. **No software power-off:** `bsp_pmic_power_off()` returns
   `ESP_ERR_NOT_SUPPORTED` because the board has no software-cuttable battery
   rail.
