#include "socket.h"

#include <string.h>

#include "../../../include/libdaikinhal.h"
#include "../../../src/trace.h"

const uint8_t TCP_SOCKET_ID = 0;
const uint8_t INVALID_SOCKET = (~((uint8_t)0));

bool daikin_hal_tcp_open(daikin_hal_tcp_t* const tcp)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    tcp->handle = (void*)((uint32_t)INVALID_SOCKET);
    const uint16_t remote_port = DAIKIN_REMOTE_PORT;

    int8_t ret = socket(TCP_SOCKET_ID, Sn_MR_TCP, 0, 0);
    if (ret != TCP_SOCKET_ID)
    {
        LIBDAIKIN_ERROR("socket error: %d.\n", ret);
        return false;
    }

    // NO Bind to a specific local network interface

    uint32_t remote_ip = daikin_hal_tcp_IPv4(DAIKIN_REMOTE_IP);
    uint8_t* addr = (uint8_t*)(&remote_ip);

    LIBDAIKIN_TRACE("CONNECTING %u.%u.%u.%u:%u.\n",
        addr[0], addr[1], addr[2], addr[3], remote_port);

    ret = connect(TCP_SOCKET_ID, addr, remote_port);
    if (ret != SOCK_OK)
    {
        LIBDAIKIN_ERROR("Unable to connect to '%s':%u.\n", DAIKIN_REMOTE_IP, remote_port);
        close(TCP_SOCKET_ID);
        return false;
    }

    tcp->handle = (void*)((uint32_t)TCP_SOCKET_ID);
    return true;
}

int32_t daikin_hal_tcp_read(
    const daikin_hal_tcp_t* const tcp,
    char* const data,
    uint16_t len)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(data != NULL);
    LIBDAIKIN_ASSERT(len > 0);

    int32_t ret = recv(TCP_SOCKET_ID, (uint8_t*)data, len);
    if (ret > 0)
        return ret;
    
    LIBDAIKIN_ERROR("recv socket error: %d.\n", ret);
    return -1;
}

int32_t daikin_hal_tcp_write(
    const daikin_hal_tcp_t* const tcp,
    const char* const data,
    uint16_t len)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(data != NULL);
    LIBDAIKIN_ASSERT(len > 0);

    int32_t ret = send(TCP_SOCKET_ID, (uint8_t*)data, len);
    if (ret > 0)
        return ret;
    
    LIBDAIKIN_ERROR("send socket error: %d.\n", ret);
    return -1;
}

void daikin_hal_tcp_close(
    daikin_hal_tcp_t* const tcp)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    uint8_t s = (uint8_t)((uint32_t)tcp->handle);

    if (s != INVALID_SOCKET) {

        // Here we do not use WIN shutdown, just disconnect
        int8_t ret = disconnect(s);
        if (ret != SOCK_OK)
        {
            LIBDAIKIN_ERROR("disconnect socket error: %d.\n", ret);
        }

        ret = close(s);
        if (ret != SOCK_OK)
        {
            LIBDAIKIN_ERROR("close error: %d.\n", ret);
        }

        LIBDAIKIN_TRACE("SOCKET '%d' CLOSED.\n", s);
        tcp->handle = (void*)((uint32_t)INVALID_SOCKET);
    }
}
