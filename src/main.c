#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/usb/usbd.h>

#include "g6_config.h"
#include "g6_hw.h"
#include "g6_input_guard.h"
#include "g6_usb.h"

#define G6_MOUSE_REPORT_SIZE 4U
#define G6_INPUT_LOOP_US 50U
#define G6_WATCHDOG_TIMEOUT_MS 1500U
#define G6_USB_RESTART_DELAY_MS 250U

static const uint8_t mouse_report_desc[] = HID_MOUSE_REPORT_DESC(8);

static const uint8_t config_report_desc[] = {
	HID_USAGE_PAGE16(0xFF00),
	HID_USAGE(0x01),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
		HID_REPORT_ID(G6_FEATURE_REPORT_ID),
		HID_LOGICAL_MIN8(0),
		HID_LOGICAL_MAX16(0xFF, 0x00),
		HID_REPORT_SIZE(8),
		HID_REPORT_COUNT(G6_FEATURE_FRAME_SIZE),
		HID_FEATURE(0x02),
	HID_END_COLLECTION,
};

static const struct device *const mouse_dev =
	DEVICE_DT_GET(DT_NODELABEL(g6_mouse_hid));
static const struct device *const config_dev =
	DEVICE_DT_GET(DT_NODELABEL(g6_config_hid));
static const struct device *const watchdog_dev =
	DEVICE_DT_GET(DT_ALIAS(watchdog0));

static struct g6_config active_config;
static struct g6_health health;
static struct g6_input_guard input_guard;
static uint8_t feature_response[G6_FEATURE_FRAME_SIZE];
static struct k_mutex state_lock;
static atomic_t mouse_ready;
static atomic_t config_ready;
static int watchdog_channel = -1;
static struct usbd_context *usb_context;
static struct k_work_delayable usb_restart_work;
static uint32_t pending_polling_period_us;

static void record_error(int ret)
{
	if (ret >= 0) {
		return;
	}
	k_mutex_lock(&state_lock, K_FOREVER);
	health.last_error = (uint32_t)-ret;
	k_mutex_unlock(&state_lock);
}

static void usb_restart_handler(struct k_work *work)
{
	uint32_t period_us = pending_polling_period_us;
	int ret;

	ARG_UNUSED(work);
	atomic_clear(&mouse_ready);
	atomic_clear(&config_ready);

	ret = usbd_disable(usb_context);
	if (ret != 0) {
		record_error(ret);
		return;
	}
	ret = usbd_shutdown(usb_context);
	if (ret != 0) {
		record_error(ret);
		return;
	}
	ret = hid_device_set_in_polling(mouse_dev, period_us);
	if (ret != 0) {
		record_error(ret);
		return;
	}
	ret = usbd_init(usb_context);
	if (ret != 0) {
		record_error(ret);
		return;
	}
	ret = usbd_enable(usb_context);
	record_error(ret);
}

static void update_health_locked(void)
{
	health.uptime_ms = k_uptime_get_32();
	health.sensor_ready = g6_hw_sensor_ready();
	health.buttons_ready = g6_hw_buttons_ready();
	health.usb_ready = atomic_get(&mouse_ready) &&
			   atomic_get(&config_ready);
	health.recovery_available = 0U;
}

static void apply_active_config_locked(uint16_t old_polling)
{
	int ret = g6_hw_apply_config(&active_config);

	if (ret != 0) {
		health.last_error = (uint32_t)-ret;
	}
	if (active_config.polling_hz != old_polling) {
		pending_polling_period_us =
			g6_polling_period_us(active_config.polling_hz);
		k_work_reschedule(&usb_restart_work,
				  K_MSEC(G6_USB_RESTART_DELAY_MS));
	}
}

static void process_control_event_locked(enum g6_control_event event)
{
	uint16_t old_polling = active_config.polling_hz;

	if (event == G6_CONTROL_CYCLE_POLLING) {
		g6_cycle_polling(&active_config);
	} else if (event == G6_CONTROL_CYCLE_DPI) {
		g6_cycle_dpi(&active_config);
	} else {
		return;
	}

	/* A mode/config shortcut must not leave a host-side pressed button. */
	g6_input_guard_reset(&input_guard);
	health.control_events++;
	apply_active_config_locked(old_polling);
}

static void iface_ready(const struct device *dev, bool ready)
{
	if (dev == mouse_dev) {
		atomic_set(&mouse_ready, ready);
	} else if (dev == config_dev) {
		atomic_set(&config_ready, ready);
	}
}

static int config_get_report(const struct device *dev, uint8_t type,
			     uint8_t id, uint16_t len, uint8_t *buf)
{
	uint16_t copy_len;

	ARG_UNUSED(dev);
	if (type != HID_REPORT_TYPE_FEATURE ||
	    id != G6_FEATURE_REPORT_ID || buf == NULL) {
		return -ENOTSUP;
	}

	copy_len = MIN(len, (uint16_t)G6_FEATURE_FRAME_SIZE);
	k_mutex_lock(&state_lock, K_FOREVER);
	memcpy(buf, feature_response, copy_len);
	k_mutex_unlock(&state_lock);
	return copy_len;
}

static int config_set_report(const struct device *dev, uint8_t type,
			     uint8_t id, uint16_t len, const uint8_t *buf)
{
	const uint8_t *frame = buf;
	enum g6_status status;
	uint16_t old_polling;

	ARG_UNUSED(dev);
	if (type != HID_REPORT_TYPE_FEATURE ||
	    id != G6_FEATURE_REPORT_ID || buf == NULL) {
		return -ENOTSUP;
	}

	if (len == G6_FEATURE_FRAME_SIZE + 1U &&
	    buf[0] == G6_FEATURE_REPORT_ID) {
		frame = &buf[1];
		len--;
	}
	if (len != G6_FEATURE_FRAME_SIZE) {
		return -EMSGSIZE;
	}

	k_mutex_lock(&state_lock, K_FOREVER);
	old_polling = active_config.polling_hz;
	update_health_locked();
	status = g6_process_command(&active_config, &health, frame,
				    feature_response);
	if (status == G6_STATUS_OK) {
		g6_input_guard_reset(&input_guard);
		apply_active_config_locked(old_polling);
	}
	k_mutex_unlock(&state_lock);
	return 0;
}

static struct hid_device_ops mouse_ops = {
	.iface_ready = iface_ready,
};

static struct hid_device_ops config_ops = {
	.iface_ready = iface_ready,
	.get_report = config_get_report,
	.set_report = config_set_report,
};

static int watchdog_start(void)
{
	struct wdt_timeout_cfg timeout = {
		.window = {
			.min = 0U,
			.max = G6_WATCHDOG_TIMEOUT_MS,
		},
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};
	int ret;

	if (!device_is_ready(watchdog_dev)) {
		return -ENODEV;
	}
	watchdog_channel = wdt_install_timeout(watchdog_dev, &timeout);
	if (watchdog_channel < 0) {
		return watchdog_channel;
	}
	ret = wdt_setup(watchdog_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
	return ret;
}

static void watchdog_feed_if_healthy(void)
{
	int ret;

	if (watchdog_channel < 0) {
		return;
	}
	ret = wdt_feed(watchdog_dev, watchdog_channel);
	k_mutex_lock(&state_lock, K_FOREVER);
	if (ret == 0) {
		health.watchdog_feeds++;
	} else {
		health.last_error = (uint32_t)-ret;
	}
	k_mutex_unlock(&state_lock);
}

static int initialize(void)
{
	struct usbd_context *usb;
	int ret;

	k_mutex_init(&state_lock);
	k_work_init_delayable(&usb_restart_work, usb_restart_handler);
	g6_config_defaults(&active_config);
	g6_input_guard_reset(&input_guard);
	g6_frame_init(feature_response, 0U, G6_CMD_GET_CONFIG);
	feature_response[7] = G6_STATUS_NOT_READY;
	g6_frame_finalize(feature_response);

	ret = g6_hw_init();
	if (ret != 0) {
		return ret;
	}

	if (!device_is_ready(mouse_dev) || !device_is_ready(config_dev)) {
		return -ENODEV;
	}
	ret = hid_device_register(mouse_dev, mouse_report_desc,
				  sizeof(mouse_report_desc), &mouse_ops);
	if (ret != 0) {
		return ret;
	}
	ret = hid_device_register(config_dev, config_report_desc,
				  sizeof(config_report_desc), &config_ops);
	if (ret != 0) {
		return ret;
	}
	ret = hid_device_set_in_polling(
		mouse_dev, g6_polling_period_us(active_config.polling_hz));
	if (ret != 0) {
		return ret;
	}

	usb = g6_usb_init();
	if (usb == NULL) {
		return -ENODEV;
	}
	usb_context = usb;
	ret = usbd_enable(usb);
	if (ret != 0) {
		return ret;
	}

	ret = watchdog_start();
	return ret == -ENODEV ? 0 : ret;
}

int main(void)
{
	uint8_t report[G6_MOUSE_REPORT_SIZE] __aligned(4);
	uint32_t last_watchdog_feed = 0U;
	int ret = initialize();

	if (ret != 0) {
		k_panic();
	}

	while (true) {
		struct g6_input_sample sample;
		enum g6_control_event control_event = G6_CONTROL_NONE;
		uint8_t filtered_buttons = 0U;
		bool guard_changed = false;
		uint32_t now;

		ret = g6_hw_poll(&sample);
		k_mutex_lock(&state_lock, K_FOREVER);
		health.input_loops++;
		if (ret != 0) {
			health.last_error = (uint32_t)-ret;
		}
		k_mutex_unlock(&state_lock);

		now = k_uptime_get_32();
		if (ret == 0) {
			guard_changed = g6_input_guard_process(
				&input_guard, now, sample.raw_buttons,
				&filtered_buttons, &control_event);
			if (control_event != G6_CONTROL_NONE) {
				k_mutex_lock(&state_lock, K_FOREVER);
				process_control_event_locked(control_event);
				health.suppressed_chord_reports++;
				k_mutex_unlock(&state_lock);
			}
		}

		if (ret == 0 && (sample.changed || guard_changed) &&
		    atomic_get(&mouse_ready)) {
			k_mutex_lock(&state_lock, K_FOREVER);
			report[0] = g6_apply_button_map(
				&active_config, filtered_buttons);
			k_mutex_unlock(&state_lock);
			report[1] = (uint8_t)CLAMP(sample.dx, -127, 127);
			report[2] = (uint8_t)CLAMP(sample.dy, -127, 127);
			report[3] = (uint8_t)sample.wheel;

			ret = hid_device_submit_report(
				mouse_dev, sizeof(report), report);
			k_mutex_lock(&state_lock, K_FOREVER);
			if (ret == 0) {
				health.usb_reports++;
			} else {
				health.usb_errors++;
				health.last_error = (uint32_t)-ret;
			}
			k_mutex_unlock(&state_lock);
		}

		if ((now - last_watchdog_feed) >=
		    (G6_WATCHDOG_TIMEOUT_MS / 3U)) {
			watchdog_feed_if_healthy();
			last_watchdog_feed = now;
		}

		k_busy_wait(G6_INPUT_LOOP_US);
	}
}
