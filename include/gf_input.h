#ifndef GF_INPUT_H
#define GF_INPUT_H

#include <stdbool.h>
#include <linux/input.h>   /* KEY_*, BTN_*, ABS_*, REL_* codes */

/*
 * Input via evdev (/dev/input/event*), so it works without X.
 * Your user needs to be in the `input` group (sudo adduser $USER input).
 * Keyboards, mice and gamepads are hot-plugged (rescanned every 2 s).
 *
 * Keyboard keys, mouse buttons (BTN_LEFT...) and gamepad buttons
 * (BTN_SOUTH, BTN_START...) all share the same code space.
 *
 * "pressed"/"released" are edge flags valid for the next update step.
 */
bool  gf_key_down(int code);
bool  gf_key_pressed(int code);
bool  gf_key_released(int code);

/* Gamepad/joystick axis normalized to -1..1 with a dead zone (ABS_X, ABS_Y,
 * ABS_RX, ABS_RY, ABS_HAT0X, ABS_HAT0Y...). */
float gf_axis(int abs_code, float deadzone);

/* Relative mouse motion since the previous update step, and a virtual
 * cursor clamped to the screen. */
float gf_mouse_dx(void);
float gf_mouse_dy(void);
float gf_mouse_wheel(void);
float gf_mouse_x(void);
float gf_mouse_y(void);

/* Called by the core loop. */
bool gf_input_init(void);
void gf_input_shutdown(void);
void gf_input_poll(void);
void gf_input_end_update(void);

#endif
