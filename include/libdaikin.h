#ifndef __LIB_DAIKIN_H__
#define __LIB_DAIKIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "libdaikinhal.h"

typedef enum
{
    PS_UNKNOWN,
    PS_ON,
    PS_STANDBY
} daikin_power_state_t;

typedef enum
{
    TM_UNKNOWN,
    TM_TARGET,
    TM_OFFSET
} daikin_temperature_mode_t;

typedef struct
{
    bool is_open;
    daikin_hal_tcp_t tcp;
} daikin_t;

typedef struct
{
    float indoor_temp;
    float outdoor_temp;
    float leaving_water_temp;
    daikin_power_state_t power_state;
    int32_t emergency_state;
    int32_t error_state;
    int32_t warning_state;

    daikin_temperature_mode_t temp_mode;
    uint8_t temp_target;
    int8_t temp_offset;
} daikin_device_info_t;

bool daikin_open(daikin_t* const daikin);
bool daikin_get_device_info(const daikin_t* const daikin, daikin_device_info_t* const info);
bool daikin_set_temp_target(const daikin_t* const daikin, uint8_t temp_target);
bool daikin_set_temp_offset(const daikin_t* const daikin, int8_t temp_offset);
bool daikin_set_power_state(const daikin_t* const daikin, daikin_power_state_t power_state);
void daikin_close(daikin_t* const daikin);

#ifdef __cplusplus
}
#endif

#endif
