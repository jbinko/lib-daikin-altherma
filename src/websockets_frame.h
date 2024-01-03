#ifndef __WEBSOCKETS_FRAME_H__
#define __WEBSOCKETS_FRAME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string>

#include "../include/libdaikinhal.h"

const uint16_t WS_SC_NORMAL_CLOSURE = 1000;

bool ws_write_close_frame(const daikin_hal_tcp_t* const tcp, uint16_t status_code, const char* const reason);
bool ws_wait_for_close_frame(const daikin_hal_tcp_t* const tcp);
bool ws_write_text_frame(const daikin_hal_tcp_t* const tcp, const std::string& text);
bool ws_wait_for_text_frame(const daikin_hal_tcp_t* const tcp, std::string& response);

#ifdef __cplusplus
}
#endif

#endif
