/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#include "Utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

uint8_t *Load8(uint8_t *p, uint8_t x)
{
    *p = x;
    return p + 1;
}

uint8_t *Load16(uint8_t *p, uint16_t x)
{
    p = Load8(p, (uint8_t)(x >> 8));
    p = Load8(p, (uint8_t)x);
    return p;
}

uint8_t *Load32(uint8_t *p, uint32_t x)
{
    p = Load16(p, (uint16_t)(x >> 16));
    p = Load16(p, (uint16_t)x);
    return p;
}

int writeFile(char *filename, char *data, int len, int append) {
    FILE *fp = fopen(filename, append ? "a+" : "w+");
    if (!fp)
        return -1;

    fwrite(data, 1, len, fp);

    fclose(fp);
    return 0;
}

void dumpHex(const uint8_t *ptr, int len)
{
    int i;
    printf("%p [%d]: ", (void *)ptr, len);
    for (i = 0; i < len; ++i) {
        printf("%.2X ", ptr[i]);
    }
    printf("\n");
}

char *getCurrentTime()
{
    static char ts[20] = { 0 };
    time_t currentTime = time(NULL);
    strftime(ts, 20, "%Y-%m-%d %H:%M:%S", localtime(&currentTime));
    return ts;
}