/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#include "Network.h"
#include "Utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

int udpInit(UDPContext *udp)
{
    if (NULL == udp || NULL == udp->dstIp || 0 == udp->dstPort) {
        LOGE("udpInit error.\n");
        return -1;
    }

    udp->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp->socket < 0) {
        LOGE("udpInit socket error.\n");
        return -1;
    }

    udp->servAddr.sin_family = AF_INET;
    udp->servAddr.sin_port = htons(udp->dstPort);
    inet_aton(udp->dstIp, &udp->servAddr.sin_addr);

    // test udp send
    int num = (int)sendto(udp->socket, "", 1, 0, (struct sockaddr *)&udp->servAddr, sizeof(udp->servAddr));
    if (num != 1) {
        LOGE("udpInit sendto test err. %d", num);
        return -1;
    }
    LOGD("UDP init successfully.\n");
    return 0;
}

int udpSend(const UDPContext *udp, const uint8_t *data, uint32_t len)
{
    ssize_t num = sendto(udp->socket, data, len, 0, (struct sockaddr *)&udp->servAddr, sizeof(udp->servAddr));
    if (num != len) {
        LOGE("sendto %s. %d %u socket[%d]\n", strerror(errno), (int)num, len, udp->socket);
        return -1;
    }

    return len;
}
