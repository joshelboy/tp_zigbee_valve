/**
 * This is a code for an ESP32-based Zigbee on/off valve device that can be controlled remotely.
 * 
 * @author Elias Josh Tilegant, Espressif Systems, Seong-Woo Kim
 * @date 01.10.2023
 * @version 0.1
 * @see https://github.com/Koenkk/zigbee-herdsman-converters
 * @see https://github.com/espressif/esp-idf
 * @see https://github.com/espressif/esp-zigbee-sdk
 * 
 * @note This code is a fork based on the esp-idf/examples/zigbee/esp_zb_light.c example code. (https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_on_off_light)
 * 
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_zb_light.h"
#include "nvs_flash.h"

// custom code
#include "driver/adc.h"
#include "driver/gpio.h"

/**
 * @note Make sure set idf.py menuconfig in zigbee component as zigbee end device!
*/
#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

static const char *TAG = "ESP_ZB_ON_OFF_VALVE";
static esp_zb_attribute_list_t *esp_zb_power_config_cluster = NULL;
static uint8_t battery_percentage_attr = 0;  // Initial value

#if 0
/* Main application customizable context.
 * Stores all settings and static values.
 */
typedef struct {
    zb_zcl_basic_attrs_ext_t basic_attr;
    zb_zcl_identify_attrs_t identify_attr;
    zb_zcl_scenes_attrs_t scenes_attr;
    zb_zcl_groups_attrs_t groups_attr;
    zb_zcl_on_off_attrs_t on_off_attr;
    zb_zcl_level_control_attrs_t level_control_attr;
} bulb_device_ctx_t;

/* Zigbee device application context storage. */
static bulb_device_ctx_t dev_ctx;
#endif

/********************* Define functions **************************/
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

/**
 * The function initializes the valve pins as output pins and sets their initial levels to OFF.
 */
void initialize_valve_pins() {
    esp_rom_gpio_pad_select_gpio(VALVE_ON_PIN);
    esp_rom_gpio_pad_select_gpio(VALVE_OFF_PIN);
    gpio_set_direction(VALVE_ON_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(VALVE_OFF_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(VALVE_ON_PIN, 0);  // Initially set to OFF
    gpio_set_level(VALVE_OFF_PIN, 1); // Initially set to OFF
}

/**
 * The function initializes the ADC with a 12-bit resolution and 0 dB attenuation for channel 6. This is the analog input channel for the ESP32-C6. See the ESP32-C6 datasheet for more information.
 */
void initialize_adc() {
    adc1_config_width(ADC_WIDTH_BIT_12);  // 12-bit ADC resolution
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_0);  // 0 dB attenuation
}

/**
 * The function "read_battery_voltage" reads the raw ADC value from channel 6, converts it to voltage,
 * and calculates the actual battery voltage using a voltage divider formula.
 * 
 * @return the battery voltage as a float value.
 */
float read_battery_voltage() {
    int adc_value = adc1_get_raw(ADC1_CHANNEL_6);
    float voltage_at_io6 = (adc_value / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
    
    // Calculate the actual battery voltage using the voltage divider formula
    float battery_voltage = voltage_at_io6 / (VOLTAGE_DIVIDER_R2 / (VOLTAGE_DIVIDER_R1 + VOLTAGE_DIVIDER_R2));
    
    return battery_voltage;
}


/**
 * The attr_cb function handles changes in attributes for the ON/OFF cluster, specifically controlling
 * a valve based on the new value.
 * 
 * @param status The status parameter indicates the status of the attribute change operation. It can be
 * used to check if the operation was successful or if there was an error.
 * @param endpoint The endpoint parameter represents the endpoint number of the Zigbee device. In
 * Zigbee, an endpoint is a logical entity within a device that can provide or consume services. Each
 * endpoint is associated with a specific cluster, which defines the functionality of the endpoint.
 * @param cluster_id The cluster_id parameter represents the ID of the Zigbee cluster that the
 * attribute belongs to. In this code snippet, it is used to check if the attribute belongs to the
 * ON/OFF cluster (ESP_ZB_ZCL_CLUSTER_ID_ON_OFF).
 * @param attr_id The attr_id parameter represents the attribute ID of the attribute that has changed.
 * @param new_value The `new_value` parameter is a pointer to the new value of the attribute that has
 * changed. It is of type `void*`, which means it can be a pointer to any type of data. In this case,
 * it is cast to a `uint8_t*` to access the value
 */
void attr_cb(uint8_t status, uint8_t endpoint, uint16_t cluster_id, uint16_t attr_id, void *new_value)
{
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        uint8_t value = *(uint8_t*)new_value;
        if (attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            /* implemented valve on/off control */
            ESP_LOGI(TAG, "on/off valve set to %hd", value);
            if (value) {
                ESP_LOGI(TAG, "turned valve on");
                gpio_set_level(VALVE_ON_PIN, 1);  // Turn ON the valve
                gpio_set_level(VALVE_OFF_PIN, 0); // Ensure OFF pin is low
            } else {
                ESP_LOGI(TAG, "turned valve off");
                gpio_set_level(VALVE_ON_PIN, 0);  // Turn OFF the valve
                gpio_set_level(VALVE_OFF_PIN, 1); // Ensure ON pin is low
            }
        }
    } else {
        /* Implement some actions if needed when other cluster changed */
        ESP_LOGI(TAG, "cluster:0x%x, attribute:0x%x changed ", cluster_id, attr_id);
    }
}

/**
 * The function `esp_zb_app_signal_handler` handles different types of Zigbee signals and performs
 * corresponding actions based on the signal type and error status.
 * 
 * @param signal_struct A pointer to a structure that contains information about the Zigbee application
 * signal.
 */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Start network steering");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %d)", err_status);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id());
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %d)", err_status);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %d, status: %d", sig_type, err_status);
        break;
    }
}

// setting zigbee device basic information, changes here also need to be changed in zigbee2mqtt adapter
char modelid[] = {13, 'E', 'S', 'P', '3', '2', 'C', '6', '.', 'V', 'a', 'l', 'v', 'e'};
char manufname[] = {9, 'E', 's', 'p', 'r', 'e', 's', 's', 'i', 'f'};

/**
 * The above function initializes and configures a Zigbee stack with a Zigbee end-device configuration,
 * creates and adds various clusters to the endpoint list, and starts the Zigbee stack.
 * 
 * @param pvParameters The `pvParameters` parameter is a void pointer that can be used to pass any
 * additional parameters or data to the `esp_zb_task` function. In this case, it is not used and can be
 * ignored.
 */
static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack with Zigbee end-device config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK); 

    /* set the on-off valve device config */
    uint8_t test_attr, test_attr2, power_src_attr;
 
    test_attr = 0;
    test_attr2 = 4;
    power_src_attr = 3; // 0x03 = battery => see https://github.com/Koenkk/zigbee-herdsman-converters/issues/3429
    // 
    /* basic cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_src_attr);
    esp_zb_cluster_update_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr2);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &modelid[0]);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &manufname[0]);

    // add battery % attribute + cluster
    //esp_zb_attribute_list_t *esp_zb_power_config_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG);
    esp_zb_power_config_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG);
    esp_zb_power_config_cluster_add_attr(esp_zb_power_config_cluster, ESP_ZB_ZCL_ATTR_POWER_CONFIGURATION_BATTERY_PERCENTAGE_REMAINING_ID, &battery_percentage_attr);


    /* identify cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &test_attr);
    /* group cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_GROUPS);
    esp_zb_groups_cluster_add_attr(esp_zb_groups_cluster, ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID, &test_attr);
    /* scenes cluster create with standard cluster + customized */
    esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(NULL);
    esp_zb_cluster_update_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID, &test_attr);
    /* on-off cluster create with standard cluster config*/
    esp_zb_on_off_cluster_cfg_t on_off_cfg;
    on_off_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    /* update basic cluster in the existed cluster list */
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_power_config_cluster(esp_zb_cluster_list, esp_zb_power_config_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    /* add created endpoint (cluster_list) to endpoint list */
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, HA_ESP_LIGHT_ENDPOINT, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID);
    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_device_add_set_attr_value_cb(attr_cb);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

/**
 * The function calculates the battery percentage based on the battery voltage, with a cap at 100% and
 * 0%.
 * 
 * @param battery_voltage The battery_voltage parameter represents the current voltage of the battery.
 * 
 * @return the battery percentage as a float value.
 */
float calculate_battery_percentage(float battery_voltage) {
    if (battery_voltage > BATTERY_VOLTAGE_MAX) {
        return 100.0;
    } else if (battery_voltage < BATTERY_VOLTAGE_MIN) {
        return 0.0;
    }

    return ((battery_voltage - BATTERY_VOLTAGE_MIN) / (BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN)) * 100.0;
}


/**
 * The app_main function initializes the Zigbee platform, sets up hardware and device initialization,
 * reads the battery voltage, calculates the battery percentage, and updates the battery percentage
 * attribute in the Zigbee cluster every 60 seconds. Updated battery percentage values arent being sent
 * to the coordinator rn. This is a known issue.
 */
void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    /* load Zigbee light_bulb platform config to initialization */
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    /* hardware related and device init */
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

    // custom code

    initialize_adc();
    initialize_valve_pins();

    while(1) {
        float battery_voltage = read_battery_voltage();
        battery_percentage_attr = (uint8_t)(calculate_battery_percentage(battery_voltage));
        float battery_percentage = calculate_battery_percentage(battery_voltage);
        ESP_LOGI(TAG, "Calculated Battery Percentage Attribute: %d", battery_percentage_attr);
        ESP_LOGI(TAG, "Battery Voltage: %fV, Battery Percentage: %.2f%%", battery_voltage, battery_percentage);

        // Update the battery percentage attribute
        esp_err_t err = esp_zb_cluster_update_attr(
            esp_zb_power_config_cluster, 
            ESP_ZB_ZCL_ATTR_POWER_CONFIGURATION_BATTERY_PERCENTAGE_REMAINING_ID, 
            &battery_percentage_attr
        );
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update battery percentage attribute");
        }

        vTaskDelay(pdMS_TO_TICKS(60000));
    }

}