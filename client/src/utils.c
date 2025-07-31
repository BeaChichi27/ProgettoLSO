#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void trim_whitespace(char* str) {
    if (!str) return;
    
    char *end;
    
    end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';
    
    
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    memmove(str, start, strlen(start) + 1);
}

int is_empty_string(const char* str) {
    if (!str) return 1;
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

void safe_strncpy(char* dest, const char* src, size_t size) {
    if (!dest || !src || size == 0) return;
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
}

void flush_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int validate_move_input(const char* input) {
    if (!input || strlen(input) != 1) return 0;
    return (input[0] >= '1' && input[0] <= '9');
}

void print_colored(const char* color, const char* message) {
    if (!color || !message) return;
    printf("%s%s%s\n", color, message, COLOR_RESET);
}

double get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}