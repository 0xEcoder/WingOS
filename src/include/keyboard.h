#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

extern char input_buffer[];

void keyboard_init(void);
void keyboard_handler(void);
void keyboard_enable(void);
void keyboard_disable(void);
#endif