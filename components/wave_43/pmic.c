#include "bsp/pmic.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "wave_43_pmic";

/* Waveshare ESP32-P4 4.3":
 * - Battery divider: R12=200k (top), R15=100k (bottom), node on GPIO22
 * - Divider ratio: Vadc = Vbat * (100k / (200k + 100k)) = Vbat / 3
 */
#define BAT_ADC_GPIO GPIO_NUM_22
#define BAT_ADC_UNIT ADC_UNIT_1
#define BAT_ADC_ATTEN ADC_ATTEN_DB_12
#define BAT_ADC_BITWIDTH ADC_BITWIDTH_12
#define BAT_ADC_MAX_RAW 4095.0f

/* ESP32-P4 ADC full-scale used here: 0..3.3V */
#define ADC_VREF_MV 3300.0f
#define BAT_DIVIDER_MULTIPLIER 3.0f

/* Battery percentage mapping */
#define BAT_EMPTY_MV 3000
#define BAT_FULL_MV 4200

/* Optional charger status pins (unknown on this board variant). */
#define CHG_STAT_GPIO GPIO_NUM_NC
#define VBUS_SENSE_GPIO GPIO_NUM_NC

static bool s_pmic_available = false;
static bool s_initialized = false;
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_channel_t s_adc_channel = ADC_CHANNEL_0;

static esp_err_t map_gpio_to_channel(gpio_num_t gpio, adc_unit_t unit,
                                     adc_channel_t *ch) {
  if (!ch)
    return ESP_ERR_INVALID_ARG;
  esp_err_t err = adc_oneshot_io_to_channel(gpio, unit, ch);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPIO%d is not valid ADC input for unit %d", gpio,
             (int)unit);
    return err;
  }
  return ESP_OK;
}

static esp_err_t read_battery_mv(uint16_t *mv) {
  if (!mv)
    return ESP_ERR_INVALID_ARG;
  if (!s_initialized || !s_adc_handle)
    return ESP_ERR_INVALID_STATE;

  int raw = 0;
  ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc_handle, s_adc_channel, &raw), TAG,
                      "adc read failed");

  if (raw < 0)
    raw = 0;
  if (raw > (int)BAT_ADC_MAX_RAW)
    raw = (int)BAT_ADC_MAX_RAW;

  float vadc_mv = ((float)raw * ADC_VREF_MV) / BAT_ADC_MAX_RAW;
  float vbat_mv = vadc_mv * BAT_DIVIDER_MULTIPLIER;

  if (vbat_mv < 0)
    vbat_mv = 0;
  if (vbat_mv > 65535.0f)
    vbat_mv = 65535.0f;

  *mv = (uint16_t)(vbat_mv + 0.5f);
  return ESP_OK;
}

esp_err_t bsp_pmic_init(void) {
  adc_oneshot_unit_init_cfg_t init_cfg = {
      .unit_id = BAT_ADC_UNIT,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

  ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_cfg, &s_adc_handle), TAG,
                      "adc unit init failed");
  ESP_RETURN_ON_ERROR(map_gpio_to_channel(BAT_ADC_GPIO, BAT_ADC_UNIT,
                                          &s_adc_channel),
                      TAG, "gpio->adc channel failed");

  adc_oneshot_chan_cfg_t chan_cfg = {
      .atten = BAT_ADC_ATTEN,
      .bitwidth = BAT_ADC_BITWIDTH,
  };
  ESP_RETURN_ON_ERROR(
      adc_oneshot_config_channel(s_adc_handle, s_adc_channel, &chan_cfg), TAG,
      "adc channel config failed");

#if CHG_STAT_GPIO != GPIO_NUM_NC
  gpio_config_t chg_cfg = {
      .pin_bit_mask = 1ULL << CHG_STAT_GPIO,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&chg_cfg), TAG, "chg stat gpio config failed");
#endif

#if VBUS_SENSE_GPIO != GPIO_NUM_NC
  gpio_config_t vbus_cfg = {
      .pin_bit_mask = 1ULL << VBUS_SENSE_GPIO,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&vbus_cfg), TAG, "vbus gpio config failed");
#endif

  s_initialized = true;
  s_pmic_available = true; /* available for telemetry, even without true PMIC */
  return ESP_OK;
}

esp_err_t bsp_pmic_power_off(void) {
  /* No software-cuttable rail known on this board. */
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_pmic_get_battery_mv(uint16_t *mv) {
  if (!s_pmic_available || !mv)
    return ESP_ERR_NOT_SUPPORTED;
  return read_battery_mv(mv);
}

esp_err_t bsp_pmic_get_battery_percent(uint8_t *pct) {
  if (!s_pmic_available || !pct)
    return ESP_ERR_NOT_SUPPORTED;

  uint16_t mv = 0;
  ESP_RETURN_ON_ERROR(read_battery_mv(&mv), TAG, "read_battery_mv failed");

  if (mv <= BAT_EMPTY_MV) {
    *pct = 0;
  } else if (mv >= BAT_FULL_MV) {
    *pct = 100;
  } else {
    uint32_t num = (uint32_t)(mv - BAT_EMPTY_MV) * 100U;
    uint32_t den = (uint32_t)(BAT_FULL_MV - BAT_EMPTY_MV);
    *pct = (uint8_t)(num / den);
  }

  return ESP_OK;
}

esp_err_t bsp_pmic_get_charge_status(bsp_pmic_chg_t *status) {
  if (!s_pmic_available || !status)
    return ESP_ERR_NOT_SUPPORTED;

#if CHG_STAT_GPIO != GPIO_NUM_NC
  /* Typical charger STAT pins are active-low when charging; adjust if needed. */
  int stat = gpio_get_level(CHG_STAT_GPIO);
  *status = (stat == 0) ? BSP_PMIC_CHG_CHARGING : BSP_PMIC_CHG_DISCHARGING;
  return ESP_OK;
#else
  *status = BSP_PMIC_CHG_DISCHARGING;
  return ESP_OK;
#endif
}

bool bsp_pmic_is_vbus_present(void) {
#if VBUS_SENSE_GPIO != GPIO_NUM_NC
  return gpio_get_level(VBUS_SENSE_GPIO) == 1;
#else
  return false;
#endif
}

bool bsp_pmic_is_available(void) { return s_pmic_available; }

bool bsp_pmic_can_power_off(void) { return false; }
