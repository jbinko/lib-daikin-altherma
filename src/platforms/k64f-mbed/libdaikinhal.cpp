#include "mbed.h"

#include <string.h>

#include "../../../include/libdaikinhal.h"
#include "../../../src/trace.h"

#define INVALID_SOCKET (NULL)

static TCPSocket socket;
extern NetworkInterface* net;

bool daikin_hal_tcp_open(daikin_hal_tcp_t* const tcp)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    tcp->handle = INVALID_SOCKET;
    const uint16_t remote_port = DAIKIN_REMOTE_PORT;

    nsapi_error_t ret = socket.open(net);
    if (ret != NSAPI_ERROR_OK)
    {
        LIBDAIKIN_ERROR("Unable to open socket. Error: %d.\n", ret);
        return false;
    }

    // Bind to a specific local network interface
    SocketAddress local_ip(DAIKIN_LOCAL_IP);
    ret = socket.bind(local_ip);
    if (ret != NSAPI_ERROR_OK)
    {
        LIBDAIKIN_ERROR("Unable to bind socket to '%s'. Error: %d.\n", local_ip.get_ip_address(), ret);
        socket.close();
        return false;
    }

    SocketAddress remote_ip(DAIKIN_REMOTE_IP);
    remote_ip.set_port(remote_port);

    LIBDAIKIN_TRACE("CONNECTING %s:%u.\n", remote_ip.get_ip_address(), remote_ip.get_port());

    ret = socket.connect(remote_ip);
    if (ret != NSAPI_ERROR_OK)
    {
        LIBDAIKIN_ERROR("Unable to connect to '%s':%u. Error: %d.\n", remote_ip.get_ip_address(), remote_ip.get_port(), ret);
        socket.close();
        return false;
    }

    tcp->handle = (void*)&socket;
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

    nsapi_size_or_error_t ret = socket.recv((void*)data, (nsapi_size_t)len);
    if (ret > 0)
        return (int32_t)ret;
    
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

    nsapi_size_or_error_t ret = socket.send((const void*)data, (nsapi_size_t)len);
    if (ret > 0)
        return (int32_t)ret;
    
    LIBDAIKIN_ERROR("send socket error: %d.\n", ret);
    return -1;
}

void daikin_hal_tcp_close(
    daikin_hal_tcp_t* const tcp)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    void* s = tcp->handle;

    if (s != INVALID_SOCKET) {

        // Here we do not use WIN shutdown, and we do not have disconnect

        nsapi_error_t ret = socket.close();
        if (ret != NSAPI_ERROR_OK)
        {
            LIBDAIKIN_ERROR("close error: %d.\n", ret);
        }

        LIBDAIKIN_TRACE("SOCKET CLOSED.\n");
        tcp->handle = INVALID_SOCKET;
    }
}
