#ifndef G6_CONFIG_H
#define G6_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define G6_PROTOCOL_VERSION 1U
#define G6_FIRMWARE_VERSION "0.2.0-dvt1"
#define G6_FEATURE_REPORT_ID 0x51U
#define G6_FEATURE_FRAME_SIZE 63U
#define G6_FEATURE_PAYLOAD_SIZE 50U

#define G6_DPI_MIN 1U
#define G6_DPI_MAX 40000U
#define G6_SENSOR_FPS_MIN 20000U
#define G6_SENSOR_FPS_MAX 20000U
#define G6_BUTTON_COUNT 8U

enum g6_command {
	G6_CMD_GET_CAPABILITIES = 0x01,
	G6_CMD_GET_CONFIG = 0x02,
	G6_CMD_SET_DPI = 0x10,
	G6_CMD_SET_POLLING = 0x11,
	G6_CMD_SET_SENSOR_FPS = 0x12,
	G6_CMD_SET_BUTTON_MAP = 0x13,
	G6_CMD_SET_SWITCH_MODE = 0x14,
	G6_CMD_SAVE = 0x20,
	G6_CMD_FACTORY_RESET = 0x21,
	G6_CMD_GET_HEALTH = 0x30,
	G6_CMD_REBOOT_RECOVERY = 0x31,
};

enum g6_status {
	G6_STATUS_OK = 0,
	G6_STATUS_BAD_MAGIC = 1,
	G6_STATUS_BAD_VERSION = 2,
	G6_STATUS_BAD_LENGTH = 3,
	G6_STATUS_BAD_CRC = 4,
	G6_STATUS_BAD_COMMAND = 5,
	G6_STATUS_BAD_VALUE = 6,
	G6_STATUS_NOT_READY = 7,
	G6_STATUS_NOT_IMPLEMENTED = 8,
};

enum g6_switch_mode {
	G6_SWITCH_OPTICAL = 0,
	G6_SWITCH_MAGNETIC = 1,
	G6_SWITCH_HYBRID = 2,
};

enum g6_button_action {
	G6_ACTION_NONE = 0,
	G6_ACTION_LEFT = 1,
	G6_ACTION_RIGHT = 2,
	G6_ACTION_MIDDLE = 3,
	G6_ACTION_BACK = 4,
	G6_ACTION_FORWARD = 5,
	G6_ACTION_DPI_UP = 6,
	G6_ACTION_DPI_DOWN = 7,
};

struct g6_config {
	uint16_t dpi;
	uint16_t polling_hz;
	uint16_t sensor_fps;
	uint8_t switch_mode;
	uint8_t button_map[G6_BUTTON_COUNT];
};

struct g6_health {
	uint32_t uptime_ms;
	uint32_t input_loops;
	uint32_t usb_reports;
	uint32_t usb_errors;
	uint32_t watchdog_feeds;
	uint32_t last_error;
	uint8_t sensor_ready;
	uint8_t buttons_ready;
	uint8_t usb_ready;
	uint8_t recovery_available;
	uint32_t control_events;
	uint32_t suppressed_chord_reports;
};

void g6_config_defaults(struct g6_config *cfg);
bool g6_config_valid(const struct g6_config *cfg);
bool g6_polling_hz_valid(uint16_t polling_hz);
uint32_t g6_polling_period_us(uint16_t polling_hz);
void g6_cycle_dpi(struct g6_config *cfg);
void g6_cycle_polling(struct g6_config *cfg);
uint8_t g6_apply_button_map(const struct g6_config *cfg, uint8_t raw_buttons);

uint32_t g6_crc32(const uint8_t *data, size_t len);
void g6_frame_init(uint8_t frame[G6_FEATURE_FRAME_SIZE],
		   uint8_t sequence, uint8_t command);
void g6_frame_finalize(uint8_t frame[G6_FEATURE_FRAME_SIZE]);
enum g6_status g6_frame_validate(
	const uint8_t frame[G6_FEATURE_FRAME_SIZE]);

enum g6_status g6_process_command(
	struct g6_config *cfg,
	const struct g6_health *health,
	const uint8_t request[G6_FEATURE_FRAME_SIZE],
	uint8_t response[G6_FEATURE_FRAME_SIZE]);

#endif
