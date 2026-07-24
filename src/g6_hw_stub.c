#include "g6_hw.h"

#include <errno.h>
#include <string.h>

/*
 * Safety stub: the G6 production GPIO/SPI map is not yet verified.
 * It deliberately drives no pins and reports no motion or button events.
 */

int g6_hw_init(void)
{
	return 0;
}

int g6_hw_apply_config(const struct g6_config *cfg)
{
	return g6_config_valid(cfg) ? 0 : -EINVAL;
}

int g6_hw_poll(struct g6_input_sample *sample)
{
	if (sample == NULL) {
		return -EINVAL;
	}

	memset(sample, 0, sizeof(*sample));
	return 0;
}

bool g6_hw_sensor_ready(void)
{
	return false;
}

bool g6_hw_buttons_ready(void)
{
	return false;
}
