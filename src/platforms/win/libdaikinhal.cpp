#define WIN32_LEAN_AND_MEAN

#include <ws2tcpip.h>

#include <string.h>

#include "../../../include/libdaikinhal.h"
#include "../../../src/trace.h"

bool daikin_hal_tcp_open(daikin_hal_tcp_t* const tcp)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    tcp->handle = (void*)INVALID_SOCKET;
    const uint16_t remote_port = DAIKIN_REMOTE_PORT;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        LIBDAIKIN_ERROR("socket error: %d.\n", WSAGetLastError());
        return false;
    }

    // Bind to a specific local network interface
    struct sockaddr_in local = { 0 };
    local.sin_family = AF_INET;
    local.sin_port = 0; // Any port
    local.sin_addr.s_addr = daikin_hal_tcp_IPv4(DAIKIN_LOCAL_IP);

    int ret = bind(s, (struct sockaddr*)&local, sizeof(local));
    if (ret == SOCKET_ERROR)
    {
        LIBDAIKIN_ERROR("bind socket error: %d.\n", WSAGetLastError());
        closesocket(s);
        return false;
    }

    struct sockaddr_in remote = { 0 };
    remote.sin_family = AF_INET;
    remote.sin_port = htons(remote_port);
    remote.sin_addr.s_addr = daikin_hal_tcp_IPv4(DAIKIN_REMOTE_IP);

    uint8_t* a = (uint8_t*)(&remote.sin_addr.s_addr);
    LIBDAIKIN_TRACE("CONNECTING %u.%u.%u.%u:%u.\n",
        a[0], a[1], a[2], a[3], remote_port);

    ret = connect(s, (struct sockaddr*)&remote, sizeof(remote));
    if (ret == SOCKET_ERROR)
    {
        LIBDAIKIN_ERROR("Unable to connect to %s:%u. Error: %d\n",
            DAIKIN_REMOTE_IP, remote_port, WSAGetLastError());
        closesocket(s);
        return false;
    }

    tcp->handle = (void*)s;
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

    int ret = recv((SOCKET)tcp->handle, data, len, 0);
    if (ret == SOCKET_ERROR)
    {
        LIBDAIKIN_ERROR("recv socket error: %d.\n", WSAGetLastError());
        return -1;
    }

    return ret;
}

int32_t daikin_hal_tcp_write(
    const daikin_hal_tcp_t* const tcp,
    const char* const data,
    uint16_t len)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(data != NULL);
    LIBDAIKIN_ASSERT(len > 0);

    int ret = send((SOCKET)tcp->handle, data, len, 0);
    if (ret == SOCKET_ERROR)
    {
        LIBDAIKIN_ERROR("send socket error: %d.\n", WSAGetLastError());
        return -1;
    }

    return ret;
}

void daikin_hal_tcp_close(
    daikin_hal_tcp_t* const tcp)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    SOCKET s = (SOCKET)tcp->handle;
    if (s != INVALID_SOCKET) {

        int ret = shutdown(s, SD_SEND);
        if (ret == SOCKET_ERROR)
        {
            LIBDAIKIN_ERROR("shutdown socket error: %d.\n", WSAGetLastError());
        }

        ret = closesocket(s);
        if (ret == SOCKET_ERROR)
        {
            LIBDAIKIN_ERROR("closesocket error: %d.\n", WSAGetLastError());
        }

        LIBDAIKIN_TRACE("SOCKET '%p' CLOSED.\n", (void*)s);
        tcp->handle = (void*)INVALID_SOCKET;
    }
}
