#ifndef G6_HW_H
#define G6_HW_H

#include <stdbool.h>
#include <stdint.h>

#include "g6_config.h"

struct g6_input_sample {
	int16_t dx;
	int16_t dy;
	int8_t wheel;
	uint8_t raw_buttons;
	bool changed;
};

int g6_hw_init(void);
int g6_hw_apply_config(const struct g6_config *cfg);
int g6_hw_poll(struct g6_input_sample *sample);
bool g6_hw_sensor_ready(void);
bool g6_hw_buttons_ready(void);

#endif
