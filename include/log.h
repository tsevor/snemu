#pragma once

#include <stdio.h>

extern int loglevel;

#define lerror(fmt, ...) (loglevel >= 0) ? fprintf(stderr, "\x1b[91m[ERROR]\x1b[m " fmt, ##__VA_ARGS__) : 0
#define lhost(fmt, ...)  (loglevel >= 1) ? fprintf(stderr, "\x1b[94m[HOST]\x1b[m "  fmt, ##__VA_ARGS__) : 0
#define lwarn(fmt, ...)  (loglevel >= 1) ? fprintf(stderr, "\x1b[93m[WARN]\x1b[m "  fmt, ##__VA_ARGS__) : 0
#define linfo(fmt, ...)  (loglevel >= 2) ? fprintf(stderr, "\x1b[96m[INFO]\x1b[m "  fmt, ##__VA_ARGS__) : 0
#define ldeb1(fmt, ...)  (loglevel >= 3) ? fprintf(stderr, "\x1b[92m[DEB1]\x1b[m "  fmt, ##__VA_ARGS__) : 0
#define ldeb2(fmt, ...)  (loglevel >= 4) ? fprintf(stderr, "\x1b[95m[DEB2]\x1b[m "  fmt, ##__VA_ARGS__) : 0
