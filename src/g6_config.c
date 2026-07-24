#include "g6_config.h"

#include <string.h>

#define G6_FRAME_MAGIC_0 'G'
#define G6_FRAME_MAGIC_1 '6'
#define G6_FRAME_MAGIC_2 'H'
#define G6_FRAME_MAGIC_3 '1'

#define G6_FRAME_VERSION_OFFSET 4U
#define G6_FRAME_SEQUENCE_OFFSET 5U
#define G6_FRAME_COMMAND_OFFSET 6U
#define G6_FRAME_STATUS_OFFSET 7U
#define G6_FRAME_LENGTH_OFFSET 8U
#define G6_FRAME_PAYLOAD_OFFSET 9U
#define G6_FRAME_CRC_OFFSET 59U

#define G6_CAP_WIRED_USB (1U << 0)
#define G6_CAP_HIGH_SPEED_USB (1U << 1)
#define G6_CAP_DYNAMIC_POLLING (1U << 2)
#define G6_CAP_BUTTON_REMAP (1U << 3)
#define G6_CAP_WATCHDOG (1U << 4)
#define G6_CAP_SENSOR_PROFILE_6E (1U << 5)
#define G6_CAP_OPTICAL_SWITCHES (1U << 6)

static uint16_t get_le16(const uint8_t *src)
{
	return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static uint32_t get_le32(const uint8_t *src)
{
	return (uint32_t)src[0] |
	       ((uint32_t)src[1] << 8) |
	       ((uint32_t)src[2] << 16) |
	       ((uint32_t)src[3] << 24);
}

static void put_le16(uint8_t *dst, uint16_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *dst, uint32_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
	dst[2] = (uint8_t)(value >> 16);
	dst[3] = (uint8_t)(value >> 24);
}

void g6_config_defaults(struct g6_config *cfg)
{
	static const uint8_t default_map[G6_BUTTON_COUNT] = {
		G6_ACTION_LEFT,
		G6_ACTION_RIGHT,
		G6_ACTION_MIDDLE,
		G6_ACTION_BACK,
		G6_ACTION_FORWARD,
		G6_ACTION_DPI_UP,
		G6_ACTION_DPI_DOWN,
		G6_ACTION_NONE,
	};

	cfg->dpi = 1600U;
	cfg->polling_hz = 1000U;
	cfg->sensor_fps = 20000U;
	cfg->switch_mode = G6_SWITCH_OPTICAL;
	memcpy(cfg->button_map, default_map, sizeof(default_map));
}

bool g6_polling_hz_valid(uint16_t polling_hz)
{
	switch (polling_hz) {
	case 125U:
	case 500U:
	case 1000U:
	case 2000U:
	case 4000U:
	case 8000U:
		return true;
	default:
		return false;
	}
}

uint32_t g6_polling_period_us(uint16_t polling_hz)
{
	return g6_polling_hz_valid(polling_hz) ?
		       (1000000U / polling_hz) : 0U;
}

void g6_cycle_dpi(struct g6_config *cfg)
{
	static const uint16_t stages[] = {400U, 800U, 1600U, 3200U, 5000U};
	size_t i;

	if (cfg == NULL) {
		return;
	}
	for (i = 0U; i < sizeof(stages) / sizeof(stages[0]); ++i) {
		if (cfg->dpi == stages[i]) {
			cfg->dpi = stages[(i + 1U) %
					(sizeof(stages) / sizeof(stages[0]))];
			return;
		}
	}
	cfg->dpi = stages[0];
}

void g6_cycle_polling(struct g6_config *cfg)
{
	static const uint16_t stages[] = {125U, 500U, 1000U, 2000U, 4000U, 8000U};
	size_t i;

	if (cfg == NULL) {
		return;
	}
	for (i = 0U; i < sizeof(stages) / sizeof(stages[0]); ++i) {
		if (cfg->polling_hz == stages[i]) {
			cfg->polling_hz = stages[(i + 1U) %
					(sizeof(stages) / sizeof(stages[0]))];
			return;
		}
	}
	cfg->polling_hz = stages[0];
}

bool g6_config_valid(const struct g6_config *cfg)
{
	size_t i;

	if (cfg == NULL ||
	    cfg->dpi < G6_DPI_MIN || cfg->dpi > G6_DPI_MAX ||
	    !g6_polling_hz_valid(cfg->polling_hz) ||
	    cfg->sensor_fps < G6_SENSOR_FPS_MIN ||
	    cfg->sensor_fps > G6_SENSOR_FPS_MAX ||
	    cfg->switch_mode > G6_SWITCH_HYBRID) {
		return false;
	}

	for (i = 0U; i < G6_BUTTON_COUNT; ++i) {
		if (cfg->button_map[i] > G6_ACTION_DPI_DOWN) {
			return false;
		}
	}

	return true;
}

uint8_t g6_apply_button_map(const struct g6_config *cfg, uint8_t raw_buttons)
{
	uint8_t mapped = 0U;
	size_t i;

	for (i = 0U; i < G6_BUTTON_COUNT; ++i) {
		uint8_t action;

		if ((raw_buttons & (1U << i)) == 0U) {
			continue;
		}

		action = cfg->button_map[i];
		if (action >= G6_ACTION_LEFT && action <= G6_ACTION_FORWARD) {
			mapped |= 1U << (action - G6_ACTION_LEFT);
		}
	}

	return mapped;
}

uint32_t g6_crc32(const uint8_t *data, size_t len)
{
	uint32_t crc = 0xFFFFFFFFU;
	size_t i;

	for (i = 0U; i < len; ++i) {
		uint32_t byte = data[i];
		unsigned int bit;

		crc ^= byte;
		for (bit = 0U; bit < 8U; ++bit) {
			uint32_t mask = 0U - (crc & 1U);
			crc = (crc >> 1) ^ (0xEDB88320U & mask);
		}
	}

	return ~crc;
}

void g6_frame_init(uint8_t frame[G6_FEATURE_FRAME_SIZE],
		   uint8_t sequence, uint8_t command)
{
	memset(frame, 0, G6_FEATURE_FRAME_SIZE);
	frame[0] = G6_FRAME_MAGIC_0;
	frame[1] = G6_FRAME_MAGIC_1;
	frame[2] = G6_FRAME_MAGIC_2;
	frame[3] = G6_FRAME_MAGIC_3;
	frame[G6_FRAME_VERSION_OFFSET] = G6_PROTOCOL_VERSION;
	frame[G6_FRAME_SEQUENCE_OFFSET] = sequence;
	frame[G6_FRAME_COMMAND_OFFSET] = command;
}

void g6_frame_finalize(uint8_t frame[G6_FEATURE_FRAME_SIZE])
{
	put_le32(&frame[G6_FRAME_CRC_OFFSET],
		 g6_crc32(frame, G6_FRAME_CRC_OFFSET));
}

enum g6_status g6_frame_validate(
	const uint8_t frame[G6_FEATURE_FRAME_SIZE])
{
	uint32_t expected;
	uint32_t actual;

	if (frame[0] != G6_FRAME_MAGIC_0 ||
	    frame[1] != G6_FRAME_MAGIC_1 ||
	    frame[2] != G6_FRAME_MAGIC_2 ||
	    frame[3] != G6_FRAME_MAGIC_3) {
		return G6_STATUS_BAD_MAGIC;
	}
	if (frame[G6_FRAME_VERSION_OFFSET] != G6_PROTOCOL_VERSION) {
		return G6_STATUS_BAD_VERSION;
	}
	if (frame[G6_FRAME_LENGTH_OFFSET] > G6_FEATURE_PAYLOAD_SIZE) {
		return G6_STATUS_BAD_LENGTH;
	}

	expected = get_le32(&frame[G6_FRAME_CRC_OFFSET]);
	actual = g6_crc32(frame, G6_FRAME_CRC_OFFSET);
	return expected == actual ? G6_STATUS_OK : G6_STATUS_BAD_CRC;
}

static void response_status(
	uint8_t response[G6_FEATURE_FRAME_SIZE], enum g6_status status)
{
	response[G6_FRAME_STATUS_OFFSET] = (uint8_t)status;
	g6_frame_finalize(response);
}

static void response_config(const struct g6_config *cfg, uint8_t *payload)
{
	put_le16(&payload[0], cfg->dpi);
	put_le16(&payload[2], cfg->polling_hz);
	put_le16(&payload[4], cfg->sensor_fps);
	payload[6] = cfg->switch_mode;
	memcpy(&payload[7], cfg->button_map, G6_BUTTON_COUNT);
}

enum g6_status g6_process_command(
	struct g6_config *cfg,
	const struct g6_health *health,
	const uint8_t request[G6_FEATURE_FRAME_SIZE],
	uint8_t response[G6_FEATURE_FRAME_SIZE])
{
	enum g6_status status = g6_frame_validate(request);
	const uint8_t *payload = &request[G6_FRAME_PAYLOAD_OFFSET];
	uint8_t *out = &response[G6_FRAME_PAYLOAD_OFFSET];
	uint8_t command = request[G6_FRAME_COMMAND_OFFSET];

	g6_frame_init(response, request[G6_FRAME_SEQUENCE_OFFSET], command);
	if (status != G6_STATUS_OK) {
		response_status(response, status);
		return status;
	}

	switch (command) {
	case G6_CMD_GET_CAPABILITIES:
		put_le16(&out[0], G6_DPI_MIN);
		put_le16(&out[2], G6_DPI_MAX);
		put_le16(&out[4], G6_SENSOR_FPS_MIN);
		put_le16(&out[6], G6_SENSOR_FPS_MAX);
		put_le16(&out[8], 0x003FU);
		out[10] = G6_BUTTON_COUNT;
		out[11] = 1U;
		put_le32(&out[12],
			 G6_CAP_WIRED_USB |
			 G6_CAP_HIGH_SPEED_USB |
			 G6_CAP_DYNAMIC_POLLING |
			 G6_CAP_BUTTON_REMAP |
			 G6_CAP_WATCHDOG |
			 G6_CAP_SENSOR_PROFILE_6E |
			 G6_CAP_OPTICAL_SWITCHES);
		response[G6_FRAME_LENGTH_OFFSET] = 16U;
		break;
	case G6_CMD_GET_CONFIG:
		response_config(cfg, out);
		response[G6_FRAME_LENGTH_OFFSET] = 15U;
		break;
	case G6_CMD_SET_DPI:
		if (request[G6_FRAME_LENGTH_OFFSET] != 2U) {
			status = G6_STATUS_BAD_LENGTH;
		} else {
			uint16_t dpi = get_le16(payload);
			if (dpi < G6_DPI_MIN || dpi > G6_DPI_MAX) {
				status = G6_STATUS_BAD_VALUE;
			} else {
				cfg->dpi = dpi;
			}
		}
		break;
	case G6_CMD_SET_POLLING:
		if (request[G6_FRAME_LENGTH_OFFSET] != 2U) {
			status = G6_STATUS_BAD_LENGTH;
		} else {
			uint16_t polling_hz = get_le16(payload);
			if (!g6_polling_hz_valid(polling_hz)) {
				status = G6_STATUS_BAD_VALUE;
			} else {
				cfg->polling_hz = polling_hz;
			}
		}
		break;
	case G6_CMD_SET_SENSOR_FPS:
		if (request[G6_FRAME_LENGTH_OFFSET] != 2U) {
			status = G6_STATUS_BAD_LENGTH;
		} else {
			uint16_t fps = get_le16(payload);
			if (fps < G6_SENSOR_FPS_MIN || fps > G6_SENSOR_FPS_MAX) {
				status = G6_STATUS_BAD_VALUE;
			} else {
				cfg->sensor_fps = fps;
			}
		}
		break;
	case G6_CMD_SET_BUTTON_MAP:
		if (request[G6_FRAME_LENGTH_OFFSET] != G6_BUTTON_COUNT) {
			status = G6_STATUS_BAD_LENGTH;
		} else {
			size_t i;
			for (i = 0U; i < G6_BUTTON_COUNT; ++i) {
				if (payload[i] > G6_ACTION_DPI_DOWN) {
					status = G6_STATUS_BAD_VALUE;
					break;
				}
			}
			if (status == G6_STATUS_OK) {
				memcpy(cfg->button_map, payload, G6_BUTTON_COUNT);
			}
		}
		break;
	case G6_CMD_SET_SWITCH_MODE:
		if (request[G6_FRAME_LENGTH_OFFSET] != 1U) {
			status = G6_STATUS_BAD_LENGTH;
		} else if (payload[0] != G6_SWITCH_OPTICAL) {
			status = G6_STATUS_BAD_VALUE;
		} else {
			cfg->switch_mode = payload[0];
		}
		break;
	case G6_CMD_GET_HEALTH:
		if (health == NULL) {
			status = G6_STATUS_NOT_READY;
			break;
		}
		put_le32(&out[0], health->uptime_ms);
		put_le32(&out[4], health->input_loops);
		put_le32(&out[8], health->usb_reports);
		put_le32(&out[12], health->usb_errors);
		put_le32(&out[16], health->watchdog_feeds);
		put_le32(&out[20], health->last_error);
		out[24] = health->sensor_ready;
		out[25] = health->buttons_ready;
		out[26] = health->usb_ready;
		out[27] = health->recovery_available;
		put_le32(&out[28], health->control_events);
		put_le32(&out[32], health->suppressed_chord_reports);
		response[G6_FRAME_LENGTH_OFFSET] = 36U;
		break;
	case G6_CMD_FACTORY_RESET:
		g6_config_defaults(cfg);
		break;
	case G6_CMD_SAVE:
	case G6_CMD_REBOOT_RECOVERY:
		status = G6_STATUS_NOT_IMPLEMENTED;
		break;
	default:
		status = G6_STATUS_BAD_COMMAND;
		break;
	}

	if (status == G6_STATUS_OK &&
	    command >= G6_CMD_SET_DPI &&
	    command <= G6_CMD_SET_SWITCH_MODE) {
		response_config(cfg, out);
		response[G6_FRAME_LENGTH_OFFSET] = 15U;
	}

	response_status(response, status);
	return status;
}
