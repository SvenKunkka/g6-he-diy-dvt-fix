#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "g6_config.h"
#include "g6_input_guard.h"

#define TEST_FRAME_LENGTH_OFFSET 8U
#define TEST_FRAME_PAYLOAD_OFFSET 9U

static void make_request(uint8_t frame[G6_FEATURE_FRAME_SIZE],
			 uint8_t command, const uint8_t *payload,
			 uint8_t payload_len)
{
	g6_frame_init(frame, 7U, command);
	frame[8] = payload_len;
	if (payload_len != 0U) {
		memcpy(&frame[9], payload, payload_len);
	}
	g6_frame_finalize(frame);
}

static void test_defaults(void)
{
	struct g6_config cfg;

	g6_config_defaults(&cfg);
	assert(g6_config_valid(&cfg));
	assert(cfg.dpi == 1600U);
	assert(cfg.polling_hz == 1000U);
	assert(cfg.sensor_fps == 20000U);
	assert(cfg.switch_mode == G6_SWITCH_OPTICAL);
}

static void test_polling_boundaries(void)
{
	assert(g6_polling_hz_valid(125U));
	assert(g6_polling_hz_valid(8000U));
	assert(!g6_polling_hz_valid(250U));
	assert(g6_polling_period_us(8000U) == 125U);
	assert(g6_polling_period_us(250U) == 0U);
}

static void test_set_dpi_and_crc(void)
{
	struct g6_config cfg;
	struct g6_health health = {0};
	uint8_t request[G6_FEATURE_FRAME_SIZE];
	uint8_t response[G6_FEATURE_FRAME_SIZE];
	uint8_t dpi[2] = {0x40, 0x9C};

	g6_config_defaults(&cfg);
	make_request(request, G6_CMD_SET_DPI, dpi, sizeof(dpi));
	assert(g6_process_command(&cfg, &health, request, response) ==
	       G6_STATUS_OK);
	assert(cfg.dpi == 40000U);
	assert(g6_frame_validate(response) == G6_STATUS_OK);

	request[20] ^= 1U;
	assert(g6_process_command(&cfg, &health, request, response) ==
	       G6_STATUS_BAD_CRC);
}

static void test_reject_invalid_without_mutation(void)
{
	struct g6_config cfg;
	struct g6_health health = {0};
	uint8_t request[G6_FEATURE_FRAME_SIZE];
	uint8_t response[G6_FEATURE_FRAME_SIZE];
	uint8_t polling[2] = {0xFA, 0x00};

	g6_config_defaults(&cfg);
	make_request(request, G6_CMD_SET_POLLING, polling, sizeof(polling));
	assert(g6_process_command(&cfg, &health, request, response) ==
	       G6_STATUS_BAD_VALUE);
	assert(cfg.polling_hz == 1000U);
}

static void test_only_verified_sensor_and_switch_modes(void)
{
	struct g6_config cfg;
	struct g6_health health = {0};
	uint8_t request[G6_FEATURE_FRAME_SIZE];
	uint8_t response[G6_FEATURE_FRAME_SIZE];
	uint8_t fps[2] = {0xC8, 0x32};
	uint8_t magnetic = G6_SWITCH_MAGNETIC;

	g6_config_defaults(&cfg);
	make_request(request, G6_CMD_SET_SENSOR_FPS, fps, sizeof(fps));
	assert(g6_process_command(&cfg, &health, request, response) ==
	       G6_STATUS_BAD_VALUE);
	assert(cfg.sensor_fps == 20000U);

	make_request(request, G6_CMD_SET_SWITCH_MODE, &magnetic, 1U);
	assert(g6_process_command(&cfg, &health, request, response) ==
	       G6_STATUS_BAD_VALUE);
	assert(cfg.switch_mode == G6_SWITCH_OPTICAL);
}

static uint16_t read_le16(const uint8_t *bytes)
{
	return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] |
	       ((uint32_t)bytes[1] << 8) |
	       ((uint32_t)bytes[2] << 16) |
	       ((uint32_t)bytes[3] << 24);
}

static void test_capabilities_match_implemented_hardware(void)
{
	struct g6_config cfg;
	struct g6_health health = {0};
	uint8_t request[G6_FEATURE_FRAME_SIZE];
	uint8_t response[G6_FEATURE_FRAME_SIZE];
	const uint8_t *payload = &response[TEST_FRAME_PAYLOAD_OFFSET];
	uint32_t capabilities;

	g6_config_defaults(&cfg);
	make_request(request, G6_CMD_GET_CAPABILITIES, NULL, 0U);
	assert(g6_process_command(&cfg, &health, request, response) ==
	       G6_STATUS_OK);
	assert(g6_frame_validate(response) == G6_STATUS_OK);
	assert(response[TEST_FRAME_LENGTH_OFFSET] == 16U);
	assert(read_le16(&payload[0]) == 1U);
	assert(read_le16(&payload[2]) == 40000U);
	assert(read_le16(&payload[4]) == 20000U);
	assert(read_le16(&payload[6]) == 20000U);
	assert(read_le16(&payload[8]) == 0x003FU);
	assert(payload[10] == G6_BUTTON_COUNT);
	assert(payload[11] == 1U);

	capabilities = read_le32(&payload[12]);
	assert(capabilities == 0x0000007FU);
	assert((capabilities & (1UL << 31)) == 0U);
}

static void test_button_mapping(void)
{
	struct g6_config cfg;

	g6_config_defaults(&cfg);
	assert(g6_apply_button_map(&cfg, 0x03U) == 0x03U);
	cfg.button_map[0] = G6_ACTION_RIGHT;
	cfg.button_map[1] = G6_ACTION_LEFT;
	assert(g6_apply_button_map(&cfg, 0x01U) == 0x02U);
	assert(g6_apply_button_map(&cfg, 0x02U) == 0x01U);
}

static void test_shortcut_chords_are_consumed(void)
{
	struct g6_input_guard guard;
	enum g6_control_event event;
	uint8_t filtered;

	g6_input_guard_reset(&guard);
	assert(!g6_input_guard_process(&guard, 0U, G6_RAW_BUTTON_MIDDLE,
				       &filtered, &event));
	assert(filtered == 0U);
	assert(event == G6_CONTROL_NONE);

	assert(!g6_input_guard_process(&guard, 10U,
				       G6_RAW_BUTTON_MIDDLE | G6_RAW_BUTTON_BACK,
				       &filtered, &event));
	assert(filtered == 0U);
	assert(event == G6_CONTROL_CYCLE_POLLING);

	assert(!g6_input_guard_process(&guard, 20U, 0U, &filtered, &event));
	assert(filtered == 0U);
	assert(event == G6_CONTROL_NONE);
}

static void test_standalone_side_button_is_delayed_not_lost(void)
{
	struct g6_input_guard guard;
	enum g6_control_event event;
	uint8_t filtered;

	g6_input_guard_reset(&guard);
	assert(!g6_input_guard_process(&guard, 0U, G6_RAW_BUTTON_FORWARD,
				       &filtered, &event));
	assert(filtered == 0U);
	assert(!g6_input_guard_process(&guard, 24U, G6_RAW_BUTTON_FORWARD,
				       &filtered, &event));
	assert(filtered == 0U);
	assert(g6_input_guard_process(&guard, 25U, G6_RAW_BUTTON_FORWARD,
				      &filtered, &event));
	assert(filtered == G6_RAW_BUTTON_FORWARD);
	assert(event == G6_CONTROL_NONE);
}

static void test_cycle_stages(void)
{
	struct g6_config cfg;

	g6_config_defaults(&cfg);
	assert(cfg.dpi == 1600U);
	g6_cycle_dpi(&cfg);
	assert(cfg.dpi == 3200U);
	cfg.dpi = 4321U;
	g6_cycle_dpi(&cfg);
	assert(cfg.dpi == 400U);

	assert(cfg.polling_hz == 1000U);
	g6_cycle_polling(&cfg);
	assert(cfg.polling_hz == 2000U);
	cfg.polling_hz = 333U;
	g6_cycle_polling(&cfg);
	assert(cfg.polling_hz == 125U);
}

int main(void)
{
	test_defaults();
	test_polling_boundaries();
	test_set_dpi_and_crc();
	test_reject_invalid_without_mutation();
	test_only_verified_sensor_and_switch_modes();
	test_capabilities_match_implemented_hardware();
	test_button_mapping();
	test_shortcut_chords_are_consumed();
	test_standalone_side_button_is_delayed_not_lost();
	test_cycle_stages();
	puts("g6_config host tests: PASS");
	return 0;
}
