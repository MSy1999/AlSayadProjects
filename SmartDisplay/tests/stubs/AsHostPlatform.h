/**
 * @file AsHostPlatform.h
 * @brief AlSayad AsHostPlatform module.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#define fileno _fileno
#endif
