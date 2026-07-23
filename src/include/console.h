#ifndef CONSOLE_H
#define CONSOLE_H

#include "limine.h"
#include "flanterm.h"

struct flanterm_context *console_createcontext(struct limine_framebuffer *fb);

extern volatile uint64_t system_ticks;

void console_init(struct limine_framebuffer *fb);
void kprintf(const char* format, ...); // Variadic function for formatted output
void klogf(const char *fmt, ...);

#endif // CONSOLE_H