/* 错误码名称（§13.4）。串口统一 `ERR <code> <NAME>`，结果行 err= 用同一套编号。 */

#include "error.h"

const char *error_name(int code)
{
    switch (code) {
    case 0:  return "OK";
    case 1:  return "OPEN";
    case 2:  return "SHORT";
    case 3:  return "CLIP";
    case 4:  return "RANGE";
    case 5:  return "CAL";
    case 6:  return "SIGNAL";
    case 7:  return "TIMEOUT";
    case 8:  return "UNCERTAIN";
    case 9:  return "CAL_S_UNSUPPORTED";
    case 10: return "HEADROOM";
    default: return "UNKNOWN";
    }
}