#ifndef UTILS_H
#define UTILS_H

#include "tris_client.h"
#include <cstddef>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

void trim_whitespace(char* str);
int is_empty_string(const char* str);
void safe_strncpy(char* dest, const char* src, size_t size);
void flush_input_buffer(void);
int validate_move_input(const char* input);
void print_colored(const char* color, const char* message);
double get_timestamp(void);

#endif 