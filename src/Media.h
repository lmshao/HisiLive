/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#ifndef HISILIVE_MEDIA_H
#define HISILIVE_MEDIA_H

#include <stdint.h>

/* copy from FFmpeg libavformat/acv.c */
const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end);

#endif  // HISILIVE_MEDIA_H
