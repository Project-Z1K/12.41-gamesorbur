#pragma once
#include <stdio.h>

// style: [INFO] filename:line (function) --> message
#define _color_error "\x1b[0;91m"
#define _color_info "\x1b[0;92m"
#define _color_warn "\x1b[0;93m"
#define _color_debug "\x1b[0;94m"
#define _color_reset "\x1b[0m"

#define _log_base(str, color, level, ...) printf(color "[%s] %s:%d (%s) --> " str _color_reset, level, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define _log_nocolor(str, level, ...) fprintf(stdout, "[%s] %s:%d (%s) --> " str, level, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

//#ifndef VPS
#define log_debug(str, ...) _log_base(str, _color_debug, "Debug", ##__VA_ARGS__)
//#else
//#define log_debug(str, ...) 
//#endif
#define log_error(str, ...) _log_base(str, _color_error, "Error", ##__VA_ARGS__)
#define log_info(str, ...) _log_base(str, _color_info, "Info", ##__VA_ARGS__)
#define log_warn(str, ...) _log_base(str, _color_warn, "Warn", ##__VA_ARGS__)
#define log_debug_nocolor(str, ...) _log_nocolor(str, "Debug", ##__VA_ARGS__)
#define log_error_nocolor(str, ...) _log_nocolor(str, "Error", ##__VA_ARGS__)
#define log_info_nocolor(str, ...) _log_nocolor(str, "Info", ##__VA_ARGS__)
#define log_warn_nocolor(str, ...) _log_nocolor(str, "Warn", ##__VA_ARGS__)
#define log_fn(str, va) fprintf(stdout, _color_info "[FN] " str _color_reset, va)
