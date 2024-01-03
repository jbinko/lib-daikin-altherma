#ifndef __LIB_DAIKIN_HAL_H__
#define __LIB_DAIKIN_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef DAIKIN_LOCAL_MAC
#   define DAIKIN_LOCAL_MAC     {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#endif

#ifndef DAIKIN_LOCAL_IP
#   define DAIKIN_LOCAL_IP      ("192.168.1.233")
#endif

#ifndef DAIKIN_LOCAL_SN
#   define DAIKIN_LOCAL_SN      ("255.255.255.0")
#endif

#ifndef DAIKIN_LOCAL_GW
#   define DAIKIN_LOCAL_GW      ("169.254.126.1")
#endif

#ifndef DAIKIN_LOCAL_DNS
#   define DAIKIN_LOCAL_DNS     ("169.254.126.1")
#endif

// Address can be assigned by DHCP.
// If DHCP cannot be reached it seem to default/fail back to 169.254.126.102 (after some time)
// If switch 4 on DIP switch is enabled - Default static IP is 169.254.10.10
#ifndef DAIKIN_REMOTE_IP
#   define DAIKIN_REMOTE_IP     ("192.168.1.135")
#endif

#ifndef DAIKIN_REMOTE_PORT
#   define DAIKIN_REMOTE_PORT   (80)
#endif

typedef struct
{
    void* handle;
} daikin_hal_tcp_t;

bool     daikin_hal_tcp_open(daikin_hal_tcp_t* const tcp); // true => success
int32_t  daikin_hal_tcp_read(const daikin_hal_tcp_t* const tcp, char* const data, uint16_t len); // Returns > 0 => success
int32_t  daikin_hal_tcp_write(const daikin_hal_tcp_t* const tcp, const char* const data, uint16_t len); // Returns > 0 => success
void     daikin_hal_tcp_close(daikin_hal_tcp_t* const tcp);

uint32_t daikin_hal_tcp_IPv4(const char* const ipv4); // Returns > 0 => success

#ifdef __cplusplus
}
#endif

#endif
