#include "g6_input_guard.h"

#include <stddef.h>

#define G6_CHORD_MASK (G6_RAW_BUTTON_MIDDLE | G6_RAW_BUTTON_BACK | \
		       G6_RAW_BUTTON_FORWARD)
#define G6_CHORD_WINDOW_MS 25U

static bool deadline_elapsed(uint32_t now_ms, uint32_t deadline_ms)
{
	return (int32_t)(now_ms - deadline_ms) >= 0;
}

void g6_input_guard_reset(struct g6_input_guard *guard)
{
	if (guard == NULL) {
		return;
	}
	guard->reported_buttons = 0U;
	guard->chord_deadline_ms = 0U;
	guard->chord_window_active = false;
	guard->combo_active = false;
}

bool g6_input_guard_process(struct g6_input_guard *guard, uint32_t now_ms,
			    uint8_t raw_buttons, uint8_t *filtered_buttons,
			    enum g6_control_event *event)
{
	uint8_t output;
	uint8_t chord;
	bool changed;

	if (guard == NULL || filtered_buttons == NULL || event == NULL) {
		return false;
	}

	*event = G6_CONTROL_NONE;
	chord = raw_buttons & G6_CHORD_MASK;

	if (guard->combo_active) {
		/* Never publish a member of a consumed chord. */
		output = raw_buttons & (uint8_t)~G6_CHORD_MASK;
		if (chord == 0U) {
			guard->combo_active = false;
		}
	} else if ((chord & G6_RAW_BUTTON_MIDDLE) != 0U &&
		   (chord & (G6_RAW_BUTTON_BACK | G6_RAW_BUTTON_FORWARD)) != 0U) {
		/* Back takes precedence if all three controls are held. */
		*event = (chord & G6_RAW_BUTTON_BACK) != 0U ?
			G6_CONTROL_CYCLE_POLLING : G6_CONTROL_CYCLE_DPI;
		guard->combo_active = true;
		guard->chord_window_active = false;
		output = raw_buttons & (uint8_t)~G6_CHORD_MASK;
	} else if (!guard->chord_window_active &&
		   ((raw_buttons ^ guard->reported_buttons) & G6_CHORD_MASK) != 0U) {
		/* Hold chord members long enough to recognise a human shortcut. */
		guard->chord_window_active = true;
		guard->chord_deadline_ms = now_ms + G6_CHORD_WINDOW_MS;
		output = (raw_buttons & (uint8_t)~G6_CHORD_MASK) |
			 (guard->reported_buttons & G6_CHORD_MASK);
	} else if (guard->chord_window_active &&
		   !deadline_elapsed(now_ms, guard->chord_deadline_ms)) {
		output = (raw_buttons & (uint8_t)~G6_CHORD_MASK) |
			 (guard->reported_buttons & G6_CHORD_MASK);
	} else {
		guard->chord_window_active = false;
		output = raw_buttons;
	}

	changed = output != guard->reported_buttons;
	guard->reported_buttons = output;
	*filtered_buttons = output;
	return changed;
}
