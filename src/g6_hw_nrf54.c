#include "g6_hw.h"
#include "g6_sensor_profile_6e.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#define SENSOR_ID_REG 0x00U
#define SENSOR_MOTION_REG 0x16U
#define SENSOR_BOOT_STATUS_REG 0x6cU
#define SENSOR_ID_PROFILE_6E 0x6eU
#define SENSOR_SPI_HZ 16000000U
#define SENSOR_SPI_GUARD_US 1U
#define SENSOR_BOOT_RETRIES 45U

BUILD_ASSERT(sizeof(g6_sensor_6e_init_pairs) == 536U);
BUILD_ASSERT(sizeof(g6_sensor_6e_post_pairs) == 8U);

static const struct device *const gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *const gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
static const struct device *const gpio2 = DEVICE_DT_GET(DT_NODELABEL(gpio2));
static const struct device *const sensor_spi =
	DEVICE_DT_GET(DT_NODELABEL(spi00));

static const uint8_t button_pins[] = {9U, 17U, 28U, 30U, 29U};
static const int8_t wheel_transition[16] = {
	0, -1, 1, 0,
	1, 0, 0, -1,
	-1, 0, 0, 1,
	0, 1, -1, 0,
};

static const struct spi_config sensor_spi_config = {
	.frequency = SENSOR_SPI_HZ,
	.operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |
		     SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA,
	.slave = 0U,
};

static bool sensor_ok;
static bool buttons_ok;
static uint8_t sensor_id;
static uint8_t last_buttons;
static uint8_t last_wheel_state;
static int8_t wheel_accumulator;
static uint32_t spi_failures;
static uint32_t next_sensor_recovery_ms;
static uint16_t active_dpi = 1600U;

static int sensor_set_dpi(uint16_t dpi);

static int sensor_select(bool selected)
{
	return gpio_pin_set_raw(gpio2, 5U, selected ? 0 : 1);
}

static int sensor_transfer(const uint8_t *tx_data, uint8_t *rx_data,
			   size_t length)
{
	struct spi_buf tx_buffer = {
		.buf = (void *)tx_data,
		.len = length,
	};
	struct spi_buf rx_buffer = {
		.buf = rx_data,
		.len = length,
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buffer,
		.count = 1U,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buffer,
		.count = 1U,
	};
	int ret;

	ret = sensor_select(true);
	if (ret != 0) {
		return ret;
	}
	k_busy_wait(SENSOR_SPI_GUARD_US);
	ret = spi_transceive(sensor_spi, &sensor_spi_config, &tx, &rx);
	k_busy_wait(SENSOR_SPI_GUARD_US);
	if (sensor_select(false) != 0 && ret == 0) {
		ret = -EIO;
	}
	if (ret != 0) {
		spi_failures++;
	}
	return ret;
}

static int sensor_write(uint8_t reg, uint8_t value)
{
	uint8_t tx[2] = {(uint8_t)(reg | BIT(7)), value};
	uint8_t rx[2];

	return sensor_transfer(tx, rx, sizeof(tx));
}

static int sensor_read(uint8_t reg, uint8_t *data, size_t length)
{
	uint8_t tx[8] = {0};
	uint8_t rx[8] = {0};
	int ret;

	if (data == NULL || length == 0U || length > 7U) {
		return -EINVAL;
	}
	tx[0] = reg & 0x7fU;
	ret = sensor_transfer(tx, rx, length + 1U);
	if (ret == 0) {
		memcpy(data, &rx[1], length);
	}
	return ret;
}

static int sensor_write_pairs(const uint8_t *pairs, size_t size)
{
	int ret;

	if (pairs == NULL || (size & 1U) != 0U) {
		return -EINVAL;
	}
	for (size_t i = 0; i < size; i += 2U) {
		ret = sensor_write(pairs[i], pairs[i + 1U]);
		if (ret != 0) {
			return ret;
		}
	}
	return 0;
}

static int sensor_reset(void)
{
	int ret;

	k_msleep(50);
	ret = gpio_pin_set_raw(gpio2, 3U, 1);
	if (ret != 0) {
		return ret;
	}
	k_msleep(1);
	ret = gpio_pin_set_raw(gpio2, 3U, 0);
	if (ret != 0) {
		return ret;
	}
	k_busy_wait(100);
	ret = gpio_pin_set_raw(gpio2, 3U, 1);
	if (ret != 0) {
		return ret;
	}
	k_msleep(10);
	return 0;
}

static int sensor_initialize_6e(void)
{
	uint8_t boot_status = 0U;
	int ret;

	ret = sensor_write_pairs(g6_sensor_6e_init_pairs,
				 sizeof(g6_sensor_6e_init_pairs));
	if (ret != 0) {
		return ret;
	}
	k_msleep(1);

	for (uint32_t attempt = 0; attempt < SENSOR_BOOT_RETRIES; attempt++) {
		ret = sensor_read(SENSOR_BOOT_STATUS_REG, &boot_status, 1U);
		if (ret == 0 && boot_status == 0x80U) {
			break;
		}
		k_busy_wait(930);
	}
	if (boot_status != 0x80U) {
		return ret != 0 ? ret : -ETIMEDOUT;
	}

	k_msleep(5);
	return sensor_write_pairs(g6_sensor_6e_post_pairs,
				  sizeof(g6_sensor_6e_post_pairs));
}

static int sensor_try_start(void)
{
	int ret;

	ret = sensor_reset();
	if (ret == 0) {
		ret = sensor_read(SENSOR_ID_REG, &sensor_id, 1U);
	}
	if (ret == 0 && sensor_id == SENSOR_ID_PROFILE_6E) {
		ret = sensor_initialize_6e();
	}
	if (ret == 0 && sensor_id != SENSOR_ID_PROFILE_6E) {
		ret = -ENOTSUP;
	}
	if (ret == 0) {
		ret = sensor_set_dpi(active_dpi);
	}
	sensor_ok = ret == 0;
	if (sensor_ok) {
		spi_failures = 0U;
	} else {
		next_sensor_recovery_ms = k_uptime_get_32() + 1000U;
	}
	return ret;
}

static int sensor_set_dpi(uint16_t dpi)
{
	uint16_t encoded;
	int ret;

	if (dpi < G6_DPI_MIN || dpi > G6_DPI_MAX) {
		return -EINVAL;
	}
	encoded = dpi - 1U;

	ret = sensor_write(0x48U, (uint8_t)encoded);
	if (ret == 0) {
		ret = sensor_write(0x49U, (uint8_t)(encoded >> 8));
	}
	if (ret == 0) {
		ret = sensor_write(0x4aU, (uint8_t)encoded);
	}
	if (ret == 0) {
		ret = sensor_write(0x4bU, (uint8_t)(encoded >> 8));
	}
	if (ret == 0) {
		ret = sensor_write(0x47U, 0x01U);
	}
	return ret;
}

static int buttons_initialize(void)
{
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(button_pins); i++) {
		ret = gpio_pin_configure(gpio1, button_pins[i],
					 GPIO_INPUT | GPIO_PULL_UP);
		if (ret != 0) {
			return ret;
		}
	}
	ret = gpio_pin_configure(gpio0, 0U, GPIO_INPUT);
	if (ret == 0) {
		ret = gpio_pin_configure(gpio0, 1U, GPIO_INPUT);
	}
	if (ret != 0) {
		return ret;
	}

	last_wheel_state = ((uint8_t)gpio_pin_get_raw(gpio0, 1U) << 1) |
			   (uint8_t)gpio_pin_get_raw(gpio0, 0U);
	return 0;
}

static int buttons_read(uint8_t *buttons)
{
	uint8_t value = 0U;
	int pin_value;

	for (size_t i = 0; i < ARRAY_SIZE(button_pins); i++) {
		pin_value = gpio_pin_get_raw(gpio1, button_pins[i]);
		if (pin_value < 0) {
			return pin_value;
		}
		if (pin_value == 0) {
			value |= BIT(i);
		}
	}
	*buttons = value;
	return 0;
}

static int wheel_read(int8_t *wheel)
{
	int a = gpio_pin_get_raw(gpio0, 1U);
	int b = gpio_pin_get_raw(gpio0, 0U);
	uint8_t state;
	int8_t delta;

	if (a < 0 || b < 0) {
		return -EIO;
	}
	state = ((uint8_t)a << 1) | (uint8_t)b;
	delta = wheel_transition[(last_wheel_state << 2) | state];
	last_wheel_state = state;
	wheel_accumulator += delta;

	*wheel = 0;
	if (wheel_accumulator >= 4) {
		*wheel = 1;
		wheel_accumulator = 0;
	} else if (wheel_accumulator <= -4) {
		*wheel = -1;
		wheel_accumulator = 0;
	}
	return 0;
}

int g6_hw_init(void)
{
	int ret;

	if (!device_is_ready(gpio0) || !device_is_ready(gpio1) ||
	    !device_is_ready(gpio2) || !device_is_ready(sensor_spi)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure(gpio2, 5U, GPIO_OUTPUT_HIGH);
	if (ret == 0) {
		ret = gpio_pin_configure(gpio2, 3U, GPIO_OUTPUT_HIGH);
	}
	if (ret != 0) {
		return ret;
	}

	ret = buttons_initialize();
	buttons_ok = ret == 0;
	if (ret != 0) {
		return ret;
	}

	(void)sensor_try_start();

	/* USB and optical buttons stay available if the sensor fails to start. */
	return 0;
}

int g6_hw_apply_config(const struct g6_config *cfg)
{
	if (!g6_config_valid(cfg)) {
		return -EINVAL;
	}
	if (!sensor_ok) {
		return -ENODEV;
	}

	/*
	 * The 0x6e sensor is capable of at least the requested 20 kFPS. The
	 * production driver exposes no literal frame-rate register; sensor_fps
	 * therefore selects a performance policy while DPI is applied here.
	 */
	active_dpi = cfg->dpi;
	return sensor_set_dpi(active_dpi);
}

int g6_hw_poll(struct g6_input_sample *sample)
{
	uint8_t motion[6];
	uint8_t buttons;
	int8_t wheel;
	int ret;

	if (sample == NULL) {
		return -EINVAL;
	}
	memset(sample, 0, sizeof(*sample));

	ret = buttons_read(&buttons);
	if (ret != 0) {
		buttons_ok = false;
		return ret;
	}
	ret = wheel_read(&wheel);
	if (ret != 0) {
		buttons_ok = false;
		return ret;
	}
	buttons_ok = true;

	if (!sensor_ok &&
	    (int32_t)(k_uptime_get_32() - next_sensor_recovery_ms) >= 0) {
		(void)sensor_try_start();
	}

	if (sensor_ok) {
		ret = sensor_read(SENSOR_MOTION_REG, motion, sizeof(motion));
		if (ret != 0) {
			sensor_ok = spi_failures < 8U;
			if (!sensor_ok) {
				next_sensor_recovery_ms = k_uptime_get_32() + 1000U;
			}
		} else {
			spi_failures = 0U;
			if ((int8_t)motion[0] < 0) {
				sample->dx = (int16_t)sys_get_le16(&motion[2]);
				sample->dy = -(int16_t)sys_get_le16(&motion[4]);
			}
		}
	}

	sample->raw_buttons = buttons;
	sample->wheel = wheel;
	sample->changed = buttons != last_buttons || wheel != 0 ||
			  sample->dx != 0 || sample->dy != 0;
	last_buttons = buttons;
	return 0;
}

bool g6_hw_sensor_ready(void)
{
	return sensor_ok;
}

bool g6_hw_buttons_ready(void)
{
	return buttons_ok;
}
