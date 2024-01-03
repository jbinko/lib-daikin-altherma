#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <winsock2.h>
#pragma comment (lib, "Ws2_32.lib")

#include "libdaikin.h"

static int cleanup_and_exit(int exit_code, daikin_t* daikin)
{
    if (daikin != NULL)
        daikin_close(daikin);

    WSACleanup();
    return exit_code;
}

int main(void)
{
    const unsigned char next_query_delay = 10;

    WSADATA wsa;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (ret != 0) {
        puts("WSAStartup error!");
        return cleanup_and_exit(-1, NULL);
    }

    daikin_t daikin = { 0 };
    daikin_device_info_t info = { 0 };

    if (daikin_open(&daikin) == false)
    {
        puts("daikin_open error!");
        return cleanup_and_exit(-2, NULL);
    }

#if WRITE
    if (daikin_set_temp_offset(&daikin, 0) == false)
    {
        puts("daikin_set_temp_offset error!");
        return cleanup_and_exit(-3, &daikin);
    }

    /*if (daikin_set_temp_target(&daikin, 21) == false)
    {
        puts("daikin_set_temp_target error!");
        return cleanup_and_exit(-4, &daikin);
    }*/

    if (daikin_set_power_state(&daikin, daikin_power_state_t::PS_ON) == false)
    {
        puts("daikin_set_power_state error!");
        return cleanup_and_exit(-5, &daikin);
    }
#endif

    while (1)
    {
        if (daikin_get_device_info(&daikin, &info) == false)
        {
            puts("daikin_get_device_info error!");
            return cleanup_and_exit(-6, &daikin);
        }

        printf("Outdoor Temperature:       %.1f\n", info.outdoor_temp);
        printf("Indoor Temperature:        %.1f\n", info.indoor_temp);
        printf("Leaving Water Temperature: %.1f\n", info.leaving_water_temp);
        printf("Target Temperature Mode:   %s\n", info.temp_mode == daikin_temperature_mode_t::TM_UNKNOWN ? "UNKNOWN" : info.temp_mode == daikin_temperature_mode_t::TM_TARGET ? "TARGET_TEMPERATURE" : "TARGET_TEMPERATURE_OFFSET");
        printf("Target Temperature:        %u\n", info.temp_target);
        printf("Target Temperature Offset: %d\n", info.temp_offset);
        printf("Power State:               %s\n", info.power_state == daikin_power_state_t::PS_UNKNOWN ? "UNKNOWN" : info.power_state == daikin_power_state_t::PS_ON ? "ON" : "STANDBY");
        printf("Emergency State:           %d\n", info.emergency_state);
        printf("Error State:               %d\n", info.error_state);
        printf("Warning State:             %d\n", info.warning_state);
        puts("");

        Sleep(next_query_delay * 1000);
    }

    return cleanup_and_exit(0, &daikin);
}
