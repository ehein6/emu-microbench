/*! \file hooks
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for hooks for timing and other region measurements
 */
#include "hooks.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

// Singleton controlling hooks output file
FILE *
hooks_output_file()
{
    static FILE* fp = NULL;
    if (fp == NULL) {
        const char* filename = getenv("HOOKS_FILENAME");
        if (filename)
        {
            fp = fopen(filename, "a");
            assert(fp);
        } else {
            // HOOKS_FILENAME unset, defaulting to stdout
            fp = fopen("/dev/stdout", "w");
        }
    }
    return fp;
}

// Singleton controlling initialization of core_clk_mhz
static long core_clk_mhz = 0;
static long get_core_clk_mhz() {
#ifndef __le64__
    (void)core_clk_mhz;
    return MEMORYWEB_X86_CLOCK_RATE;
#else
    if (core_clk_mhz == 0) {
        const char* str = getenv("CORE_CLK_MHZ");
        if (str){
            core_clk_mhz = atol(str);
        } else {
            core_clk_mhz = 150;
        }
    }
    return core_clk_mhz;
#endif
}

// The simulator can only handle one region of interest
// This global variable controls which region will get starttiming() called
const char* hooks_active_region = NULL;

void hooks_set_active_region(const char* name)
{
    hooks_active_region = name;
}

static bool region_is_active(const char* name)
{
    if (hooks_active_region == NULL) return true;
    else { return (bool)!strcmp(name, hooks_active_region); }
}

// Global timer variable
static int64_t hooks_timestamp = 0;

// Stores string that will be printed at the end of this region
#define HOOKS_STR_LEN 256
typedef struct hooks_data {
    char str[HOOKS_STR_LEN];
    size_t pos;
} hooks_data;
static hooks_data data = {};

// Append to buffer with printf
static void
hooks_data_append(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    assert(data.pos < HOOKS_STR_LEN);
    data.pos += vsprintf(data.str + data.pos, fmt, args);
    assert(data.pos < HOOKS_STR_LEN);
    va_end(args);
}

// Append JSON field to string
static void
hooks_add_field(const char* key, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (strlen(data.str) == 0) {
        // Begin JSON object
        hooks_data_append("{");
    } else {
        // Comma after previous field
        hooks_data_append(",");
    }
    // Print key
    hooks_data_append("\"%s\":", key);
    // Do printf from arguments
    assert(data.pos < HOOKS_STR_LEN);
    data.pos += vsprintf(data.str + data.pos, fmt, args);
    assert(data.pos < HOOKS_STR_LEN);
    va_end(args);
}

void hooks_region_begin(const char* name)
{
    // Add region name to the result string
    hooks_add_field("region_name", "\"%s\"", name);

    if (region_is_active(name)) {
        starttiming();
    }
    MIGRATE(&hooks_timestamp);

    // Start the timer
    hooks_timestamp = -CLOCK();
}

double hooks_region_end()
{
    // Stop the timer
    MIGRATE(&hooks_timestamp);
    hooks_timestamp += CLOCK();

    // Add time elapsed to result string
    double time_ms = (1000.0 * hooks_timestamp) / (get_core_clk_mhz()*1e6);
    hooks_add_field("time_ms", "%3.2f", time_ms);
    hooks_add_field("ticks", "%li", hooks_timestamp);

    // Dump results
    fprintf(hooks_output_file(), "%s}\n", data.str); fflush(hooks_output_file());
    memset(&data, 0, sizeof(hooks_data));

    return time_ms;
}

void hooks_set_attr_u64(const char * key, uint64_t value) { hooks_add_field(key, "%lu", value); }
void hooks_set_attr_i64(const char * key, int64_t value) { hooks_add_field(key, "%li", value); }
void hooks_set_attr_f64(const char * key, double value) { hooks_add_field(key, "%f", value); }
void hooks_set_attr_str(const char * key, const char* value) { hooks_add_field(key, "\"%s\"", value);}
void hooks_traverse_edges(uint64_t n) {}
