#include <stdint.h>
#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

char *strcpy(char *dest, const char *src) {
    char *out = dest;
    while ((*dest++ = *src++) != '\0') {
    }
    return out;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *out = dest;
    for (size_t i = 0; i < n; i++) {
        dest[i] = src[i];
        if (src[i] == '\0') {
            while (i < n) {
                dest[i++] = '\0';
            }
            break;
        }
    }
    return out;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        unsigned char c1 = (unsigned char)*s1;
        unsigned char c2 = (unsigned char)*s2;

        if (c1 != c2) {
            return (int)c1 - (int)c2;
        }
        if (c1 == '\0') {
            return 0;
        }

        s1++;
        s2++;
        n--;
    }
    return 0;
}

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}