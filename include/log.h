#pragma once

#include <stdio.h>

extern int loglevel;

#define lerror(fmt, ...) do { if (loglevel >= 0) fprintf(stderr, "\x1b[91mERROR: " fmt "\x1b[m", ##__VA_ARGS__); } while(0)
#define lhost(fmt, ...)  do { if (loglevel >= 1) fprintf(stderr, "\x1b[94mHOST: "  fmt "\x1b[m", ##__VA_ARGS__); } while(0)
#define lwarn(fmt, ...)  do { if (loglevel >= 1) fprintf(stderr, "\x1b[93mWARN: "  fmt "\x1b[m", ##__VA_ARGS__); } while(0)
#define linfo(fmt, ...)  do { if (loglevel >= 2) fprintf(stderr, "\x1b[96mINFO: "  fmt "\x1b[m", ##__VA_ARGS__); } while(0)
#define ldeb1(fmt, ...)  do { if (loglevel >= 3) fprintf(stderr, "\x1b[92mDEB1: "  fmt "\x1b[m", ##__VA_ARGS__); } while(0)
#define ldeb2(fmt, ...)  do { if (loglevel >= 4) fprintf(stderr, "\x1b[95mDEB2: "  fmt "\x1b[m", ##__VA_ARGS__); } while(0)
