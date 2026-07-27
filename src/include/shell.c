#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "shell.h"
#include "keyboard.h"
#include "string.h"
#include "timer.h"

void shell_execute(const char *command, const char *prefix) {
    if (command == NULL || command[0] == '\0') {
        kprintf("%s", prefix); // Print the prompt again if no command is entered
        return;
    }

    if (strcmp(command, "help") == 0) {
        kprintf("shell: Available commands:\n");
        kprintf("  help - Show this help message\n");
        kprintf("  clear - Clear the console\n");
        kprintf("  uptime - Show system uptime in milliseconds\n");
        kprintf("  echo <text> - Print text back to the screen\n");
    } else if (strcmp(command, "clear") == 0) {
        // Clear the console by sending ANSI escape codes
      kprintf("\033[2J\033[H"); // Clear screen and move cursor to home position
    } else if (strcmp(command, "uptime") == 0) {
        klogf("\n");
    } else if (command[0] == 'e' && command[1] == 'c' && command[2] == 'h' && command[3] == 'o' && command[4] == ' ') {
        // ECHO COMMAND: Print everything after "echo "
        const char *message = command + 5; // Skip past "echo "
        kprintf("%s\n", message);
    } else {
        kprintf("shell: Unknown command '%s'. Type 'help' for a list of commands.\n", command);
    }

    kprintf("%s", prefix); 
}