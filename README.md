# lib-daikin-altherma

This project is a C library adaptation that can communicate
with Daikin Altherma through LAN Ethernet adapter BRP069A61 or BRP069A62.

This is designed for embedded devices, but can be applied in any C/C++ project.

Project has been verified in practice with
[W5500-EVB-Pico](https://docs.wiznet.io/Product/iEthernet/W5500/w5500-evb-pico),
[FRDM-K64F](https://os.mbed.com/platforms/FRDM-K64F/)
and Windows 11.

## Example

The following is an example of how to communicate with the Daikin device.

``` cpp
#include "libdaikin.h"

daikin_t daikin = { 0 };
daikin_device_info_t info = { 0 };

if (daikin_open(&daikin) == false)
{
    puts("daikin_open error!");
    return;
}

if (daikin_get_device_info(&daikin, &info) == false)
{
    puts("daikin_get_device_info error!");
    daikin_close(&daikin);
    return;
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

if (daikin_set_temp_offset(&daikin, 2) == false)
{
    puts("daikin_set_temp_offset error!");
    daikin_close(&daikin);
    return;
}

if (daikin_set_temp_target(&daikin, 21) == false)
{
    puts("daikin_set_temp_target error!");
    daikin_close(&daikin);
    return;
}

if (daikin_set_power_state(&daikin, daikin_power_state_t::PS_ON) == false)
{
    puts("daikin_set_power_state error!");
    daikin_close(&daikin);
    return;
}

daikin_close(&daikin);
```

## Temperature Mode

Depending on your configuration, your Daikin device may use one of these temperature modes/set points.

- Desired room temperature (TM_TARGET)
- Relative value based on weather-dependent curve (TM_OFFSET)

To prevent errors, you need to use only the appropriate functions
for your current temperature mode.

- daikin_set_temp_offset (TM_OFFSET)
- daikin_set_temp_target (TM_TARGET)

## Building

Use and compile all the files that work on any platform from `src` and `include` folder.

The files that depend on the platform are in `src/platform` folder.

You should only use and compile the files that match your platform.

If you want to support a new platform, you need to write the HAL specific functions.
Look at the `include/daikin_hal.h` file for more information.

``` cpp
bool     daikin_hal_tcp_open(daikin_hal_tcp_t* const tcp); // true => success
int32_t  daikin_hal_tcp_read(const daikin_hal_tcp_t* const tcp, char* const data, uint16_t len); // Returns > 0 => success
int32_t  daikin_hal_tcp_write(const daikin_hal_tcp_t* const tcp, const char* const data, uint16_t len); // Returns > 0 => success
void     daikin_hal_tcp_close(daikin_hal_tcp_t* const tcp);
```

## Releases

- Version 1.0.0 - Initial Version. Code complete and tested.

## Notes

This module has been successfully tested with following unites:

- Daikin HVAC controller BRP069A62

## Contributing

If you want to contribute to this project, please contact me.

I'm happy to include your contributions, new features, bug fixes, new platforms, etc.

## Acknowledgments

This project was inspired by this one:
[https://github.com/Frankkkkk/python-daikin-altherma](https://github.com/Frankkkkk/python-daikin-altherma)

Dotnet version of the library is available here: 
[https://github.com/jbinko/dotnet-daikin-altherma](https://github.com/jbinko/dotnet-daikin-altherma)
