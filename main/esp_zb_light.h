/**
 * This is a header for an ESP32-based Zigbee on/off valve device that can be controlled remotely.
 * 
 * @author Elias Josh Tilegant, Espressif Systems, Seong-Woo Kim
 * @date 01.10.2023
 * @version 0.1
 * @see https://github.com/Koenkk/zigbee-herdsman-converters
 * @see https://github.com/espressif/esp-idf
 * @see https://github.com/espressif/esp-zigbee-sdk
 * 
 * @note This code is a fork based on the esp-idf/examples/zigbee/esp_zb_light.h example code. (https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_on_off_light)
 * 
 */

#include "esp_zigbee_core.h"

/* ADC config */
#define VOLTAGE_DIVIDER_R1 10000.0  // 10k resistor
#define VOLTAGE_DIVIDER_R2 4700.0   // 4.7k resistor
#define ADC_MAX_VALUE      4095.0  // 12-bit ADC
#define ADC_REFERENCE_VOLTAGE 3.3
#define BATTERY_VOLTAGE_MIN 2.5 // 18650 battery minimum voltage
#define BATTERY_VOLTAGE_MAX 4.2 // max
// #define ESP_ZB_ZCL_ATTR_BASIC_BATTERY_PERCENTAGE_ID 0x4000  
#define ESP_ZB_ZCL_ATTR_POWER_CONFIGURATION_BATTERY_PERCENTAGE_REMAINING_ID 0x0021  // standard Zigbee attribute ID for battery percentage

// valve pins
#define VALVE_ON_PIN  GPIO_NUM_4 // v on
#define VALVE_OFF_PIN GPIO_NUM_5 // v off

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false    /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define HA_ESP_LIGHT_ENDPOINT           10    /* esp light bulb device endpoint, used to process light controlling commands */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */
#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = RADIO_MODE_NATIVE,                        \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,      \
    }
