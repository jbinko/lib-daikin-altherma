#include <stdio.h>
#include <tusb.h>

#include "hardware/watchdog.h"

#include "libdaikin.h"

#include "port_common.h"
#include "wizchip_conf.h"
#include "w5x00_spi.h"

#define PLL_SYS_KHZ (133 * 1000)

static void set_clock_khz(void)
{
    set_sys_clock_khz(PLL_SYS_KHZ, true);
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}

static void wiznet_init(const wiz_NetInfo net_info)
{
    wizchip_spi_initialize();
    wizchip_cris_initialize();
    wizchip_reset();
    wizchip_initialize();
    wizchip_check();
    network_initialize(net_info);
}

static void software_reset()
{
    puts("REBOOTING...");
    watchdog_enable(100, false);
    while (1);
}

void main(void)
{
    // Wait for N seconds, before Daiking timeouts on DHCP and fails back to default IP.
    const unsigned char dhcp_timeout_delay = 35;
    const unsigned char next_query_delay = 10;

    set_clock_khz();
    stdio_init_all();

    /*
    while (!tud_cdc_connected())
        tight_loop_contents();
    */

    // https://forum.wiznet.io/t/topic/10232
    const wiz_NetInfo net_info =
    {
        .mac = DAIKIN_LOCAL_MAC,
        .ip = daikin_hal_tcp_IPv4(DAIKIN_LOCAL_IP),
        .sn = daikin_hal_tcp_IPv4(DAIKIN_LOCAL_SN),
        .gw = daikin_hal_tcp_IPv4(DAIKIN_LOCAL_GW),
        .dns = daikin_hal_tcp_IPv4(DAIKIN_LOCAL_DNS),
        .dhcp = NETINFO_STATIC
    };

    wiznet_init(net_info);
    print_network_information(net_info);

    printf("Wait for %u seconds, before Daiking timeouts on DHCP and fails back to default IP.\n", dhcp_timeout_delay);
    sleep_ms(dhcp_timeout_delay * 1000);

    daikin_t daikin = { 0 };
    daikin_device_info_t info = { 0 };

    // Try to connect.
    // If not successful -> reboot.
    // For any later error reboot as well.
    //
    // When Daikin adapter is in the DHCP mode it might not be able to reach DHCP and receive valid IP address.
    // In this case It will fail back to default IP address after some time.
    // So, we might be able to connect eventually after some time and few reboots.
    if (daikin_open(&daikin) == false)
    {
        puts("daikin_open error!");
        goto reboot;
    }

    while (1)
    {
        if (daikin_get_device_info(&daikin, &info) == false)
        {
            puts("daikin_get_device_info error!");
            goto daikin_close_reboot;
        }

        printf("Outdoor Temperature:       %.1f\n", info.outdoor_temp);
        printf("Indoor Temperature:        %.1f\n", info.indoor_temp);
        printf("Leaving Water Temperature: %.1f\n", info.leaving_water_temp);
        printf("Target Temperature Mode:   %s\n", info.temp_mode == TM_UNKNOWN ? "UNKNOWN" : info.temp_mode == TM_TARGET ? "TARGET_TEMPERATURE" : "TARGET_TEMPERATURE_OFFSET");
        printf("Target Temperature:        %u\n", info.temp_target);
        printf("Target Temperature Offset: %d\n", info.temp_offset);
        printf("Power State:               %s\n", info.power_state == PS_UNKNOWN ? "UNKNOWN" : info.power_state == PS_ON ? "ON" : "STANDBY");
        printf("Emergency State:           %d\n", info.emergency_state);
        printf("Error State:               %d\n", info.error_state);
        printf("Warning State:             %d\n", info.warning_state);
        puts("");

        sleep_ms(next_query_delay * 1000);
    }

daikin_close_reboot:
    daikin_close(&daikin);
reboot:
    software_reset();
}
