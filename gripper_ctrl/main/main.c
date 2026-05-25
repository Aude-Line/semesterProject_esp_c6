#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "led_strip.h"
#include "driver/gpio.h"

#define CTRL_GPIO   6
#define LED_GPIO    8
#define LED_WHITE_R 20
#define LED_WHITE_G 20
#define LED_WHITE_B 20
#define LED_RED_R   20
#define LED_RED_G   0
#define LED_RED_B   0

static led_strip_handle_t led_strip;

static void led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    ESP_ERROR_CHECK(gpio_reset_pin(CTRL_GPIO));
    ESP_ERROR_CHECK(gpio_set_direction(CTRL_GPIO, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(CTRL_GPIO, 0));
}

static void set_output_state(bool agent_connected, bool pin_on)
{
    if (!agent_connected) {
        ESP_ERROR_CHECK(gpio_set_level(CTRL_GPIO, 0));
        led_strip_set_pixel(led_strip, 0, LED_RED_R, LED_RED_G, LED_RED_B);
        led_strip_refresh(led_strip);
    } else if (pin_on) {
        ESP_ERROR_CHECK(gpio_set_level(CTRL_GPIO, 1));
        led_strip_set_pixel(led_strip, 0, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
        led_strip_refresh(led_strip);
    } else {
        ESP_ERROR_CHECK(gpio_set_level(CTRL_GPIO, 0));
        led_strip_clear(led_strip);
    }
}

#include <uros_network_interfaces.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/int32.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
#include <rmw_microros/rmw_microros.h>
#endif

#ifndef CONFIG_MICRO_ROS_APP_STACK
#define CONFIG_MICRO_ROS_APP_STACK 16000
#endif

#ifndef CONFIG_MICRO_ROS_APP_TASK_PRIO
#define CONFIG_MICRO_ROS_APP_TASK_PRIO 5
#endif

#define AGENT_PING_TIMEOUT_MS 100
#define AGENT_PING_ATTEMPTS   1
#define AGENT_LOST_MAX        3
#define RECONNECT_DELAY_MS    300

static std_msgs__msg__Int32 recv_msg;
static bool output_enabled = false;

static void subscription_callback(const void * msgin)
{
    const std_msgs__msg__Int32 * msg = (const std_msgs__msg__Int32 *)msgin;

    if (msg->data != 0 && msg->data != 1) {
        printf("Invalid command: %d (expected 0 or 1). Ignored.\n", (int)msg->data);
        return;
    }

    printf("Received value: %d\n", (int)msg->data);
    output_enabled = (msg->data != 0);
    set_output_state(true, output_enabled);
}

void micro_ros_task(void * arg)
{
    (void)arg;

    rcl_allocator_t allocator = rcl_get_default_allocator();

    while (1) {
        rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
        rclc_support_t support = {0};
        rcl_node_t node = rcl_get_zero_initialized_node();
        rcl_subscription_t subscriber = rcl_get_zero_initialized_subscription();
        rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
        bool connected = false;

        output_enabled = false;
        set_output_state(false, false);

        if (rcl_init_options_init(&init_options, allocator) != RCL_RET_OK) {
            goto reconnect;
        }

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
        rmw_init_options_t * rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
        if (rmw_uros_options_set_udp_address(CONFIG_MICRO_ROS_AGENT_IP, CONFIG_MICRO_ROS_AGENT_PORT, rmw_options) != RCL_RET_OK) {
            goto reconnect;
        }
#endif

        if (rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator) != RCL_RET_OK) {
            goto reconnect;
        }
        if (rclc_node_init_default(&node, "esp32_minimal_subscriber", "", &support) != RCL_RET_OK) {
            goto reconnect;
        }
        if (rclc_subscription_init_default(
                &subscriber,
                &node,
                ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
                "esp32_rx_int32") != RCL_RET_OK) {
            goto reconnect;
        }
        if (rclc_executor_init(&executor, &support.context, 1, &allocator) != RCL_RET_OK) {
            goto reconnect;
        }
        if (rclc_executor_set_timeout(&executor, RCL_MS_TO_NS(10)) != RCL_RET_OK) {
            goto reconnect;
        }
        if (rclc_executor_add_subscription(
                &executor,
                &subscriber,
                &recv_msg,
                &subscription_callback,
                ON_NEW_DATA) != RCL_RET_OK) {
            goto reconnect;
        }

        connected = true;
        set_output_state(true, output_enabled);
        printf("Connected to agent. Listening on topic: /esp32_rx_int32\n");

        {
            int missed_pings = 0;
            while (1) {
                rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

                if (rmw_uros_ping_agent(AGENT_PING_TIMEOUT_MS, AGENT_PING_ATTEMPTS) == RMW_RET_OK) {
                    missed_pings = 0;
                } else {
                    missed_pings++;
                    if (missed_pings >= AGENT_LOST_MAX) {
                        printf("Agent connection lost. Entering safe state and reconnecting...\n");
                        output_enabled = false;
                        set_output_state(false, false);
                        break;
                    }
                }

                usleep(1000);
            }
        }

reconnect:
    rcl_ret_t fini_rc = RCL_RET_OK;
        (void)rclc_executor_fini(&executor);
    fini_rc = rcl_subscription_fini(&subscriber, &node);
    (void)fini_rc;
    fini_rc = rcl_node_fini(&node);
    (void)fini_rc;
        (void)rclc_support_fini(&support);
    fini_rc = rcl_init_options_fini(&init_options);
    (void)fini_rc;
        rcl_reset_error();

        if (!connected) {
            printf("Agent unavailable. Retrying in %d ms...\n", RECONNECT_DELAY_MS);
        }
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
    }
}

void app_main(void)
{
    led_init();
    set_output_state(false, false);

#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
    ESP_ERROR_CHECK(uros_network_interface_initialize());
#endif

    xTaskCreate(
        micro_ros_task,
        "uros_task",
        CONFIG_MICRO_ROS_APP_STACK,
        NULL,
        CONFIG_MICRO_ROS_APP_TASK_PRIO,
        NULL);
}
