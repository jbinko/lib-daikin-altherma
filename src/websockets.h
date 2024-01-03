#ifndef __WEBSOCKETS_H__
#define __WEBSOCKETS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <string>

#include "../include/libdaikin.h"

bool daikin_ws_open(daikin_t* const daikin);
bool daikin_ws_request(const daikin_t* const daikin, const std::string& request, std::string& response);
void daikin_ws_close(daikin_t* const daikin);

#ifdef __cplusplus
}
#endif

#endif
