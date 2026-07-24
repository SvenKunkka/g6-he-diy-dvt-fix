#ifndef G6_INPUT_GUARD_H
#define G6_INPUT_GUARD_H

#include <stdbool.h>
#include <stdint.h>

/* Raw board button bit positions: left, right, middle, back, forward. */
#define G6_RAW_BUTTON_MIDDLE (1U << 2)
#define G6_RAW_BUTTON_BACK (1U << 3)
#define G6_RAW_BUTTON_FORWARD (1U << 4)

enum g6_control_event {
	G6_CONTROL_NONE = 0,
	G6_CONTROL_CYCLE_POLLING,
	G6_CONTROL_CYCLE_DPI,
};

/*
 * The production DVT reports ghost side-button input while the physical
 * middle+side shortcuts are used. This guard delays publication of chord
 * members briefly, consumes a recognised shortcut, and always emits a
 * neutral release when a consumed chord ends.
 */
struct g6_input_guard {
	uint8_t reported_buttons;
	uint32_t chord_deadline_ms;
	bool chord_window_active;
	bool combo_active;
};

void g6_input_guard_reset(struct g6_input_guard *guard);
bool g6_input_guard_process(struct g6_input_guard *guard, uint32_t now_ms,
			    uint8_t raw_buttons, uint8_t *filtered_buttons,
			    enum g6_control_event *event);

#endif
