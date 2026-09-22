#ifndef CONTROLS_H
#define CONTROLS_H

#include <SDL2/SDL.h>

extern volatile int virtual_test_button;
extern volatile int virtual_coin1;
extern volatile int virtual_start1;
extern volatile int virtual_service;
extern volatile int virtual_view;
extern volatile int virtual_siren;
extern volatile int virtual_volume_up;
extern volatile int virtual_volume_down;
extern volatile int virtual_estop;
extern volatile int virtual_steer_left;
extern volatile int virtual_steer_right;
extern volatile int virtual_accel;
extern volatile int virtual_brake;
extern volatile int virtual_gear_up;   /* Championship Tuning Race: shift up   */
extern volatile int virtual_gear_down; /* Championship Tuning Race: shift down */
extern volatile int input_thread_running;

int controls_read_test_button(void *self);
int controls_read_port(void *self, int port, char *value);

/* Ring Riders (joc2003) I/O emulation - see controls.c. */
int ringriders_inport(int port, char *value);
int ringriders_intf_command(int cmd, int *value);
void ringriders_coin_pulse(void);

/* Championship Tuning Race (joc2004) I/O emulation - see controls.c. */
int champrace_inport(int port, char *value);
int champrace_intf_command(int cmd, int *value);
void champrace_coin_pulse(void);
void controls_handle_event(const SDL_Event *event);
void controls_start_input_thread(void);
void controls_stop_input_thread(void);
void controls_reset_input_state(void);

#endif
