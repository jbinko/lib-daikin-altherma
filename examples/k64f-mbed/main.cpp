/*
  NOTE: To print floats use:
  mbed_app.json file by overriding the parameter target.printf_lib with the value std as shown below:
  "target_overrides": {
    "*": {
      "target.printf_lib": "std"
    }
  }
*/

#include "mbed.h"

#include "lib/lib-daikin-altherma-private/include/libdaikin.h"

NetworkInterface* net;

static void print_network_info()
{
    SocketAddress a;
    net->get_ip_address(&a);
    printf("IP address: %s\r\n", a.get_ip_address() ? a.get_ip_address() : "None");
    net->get_netmask(&a);
    printf("Netmask: %s\r\n", a.get_ip_address() ? a.get_ip_address() : "None");
    net->get_gateway(&a);
    printf("Gateway: %s\r\n", a.get_ip_address() ? a.get_ip_address() : "None");
}

static void software_reset()
{
    puts("REBOOTING...");
    Watchdog& watchdog = Watchdog::get_instance();
    watchdog.start(100);
    while (1);
}

int main()
{
    // Wait for N seconds, before Daiking timeouts on DHCP and fails back to default IP.
    const unsigned char dhcp_timeout_delay = 35;
    const unsigned char next_query_delay = 10;

    daikin_t daikin = { 0 };
    daikin_device_info_t info = { 0 };

    net = NetworkInterface::get_default_instance();
    if (net == NULL)
    {
        puts("NO NETWORK!");
        software_reset();
        return -1;
    }

    SocketAddress ip(DAIKIN_LOCAL_IP);
    SocketAddress sn(DAIKIN_LOCAL_SN);
    SocketAddress gw(DAIKIN_LOCAL_GW);
    if (net->set_network(ip, sn, gw) != NSAPI_ERROR_OK)
    {
        puts("set_network error!");
        goto reboot;
    }

    if (net->connect() != NSAPI_ERROR_OK)
    {
        puts("connect error!");
        goto reboot;
    }

    print_network_info();
    printf("Wait for %u seconds, before Daiking timeouts on DHCP and fails back to default IP.\n", dhcp_timeout_delay);
    thread_sleep_for(dhcp_timeout_delay * 1000);

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

        thread_sleep_for(next_query_delay * 1000);
    }

daikin_close_reboot:
    daikin_close(&daikin);
reboot:
    net->disconnect();
    software_reset();
    return 0;
}
