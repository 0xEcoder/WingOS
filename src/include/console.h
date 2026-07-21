#ifndef CONSOLE_H
#define CONSOLE_H

#include "limine.h"
#include "flanterm.h"

struct flanterm_context *console_createcontext(struct limine_framebuffer *fb);
void console_init(struct limine_framebuffer *fb);
void kprintf(const char* format, ...); // Variadic function for formatted output

#endif // CONSOLE_H