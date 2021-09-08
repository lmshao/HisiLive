/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#ifndef HISILIVE_NETWORK_H
#define HISILIVE_NETWORK_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct {
    char dstIp[16];
    int dstPort;
    struct sockaddr_in servAddr;
    int socket;
} UDPContext;

/* create UDP socket */
int udpInit(UDPContext *udp);

/* send UDP packet */
int udpSend(const UDPContext *udp, const uint8_t *data, uint32_t len);

#endif  // HISILIVE_NETWORK_H
