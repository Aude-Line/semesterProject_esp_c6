#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "led_strip.h"
#include "driver/gpio.h"

#define CTRL_GPIO   6
#define LED_GPIO    8
#define LED_R       20
#define LED_G       20
#define LED_B       20

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

static void led_set(bool on)
{
    if (on) {
        ESP_ERROR_CHECK(gpio_set_level(CTRL_GPIO, 1));
        led_strip_set_pixel(led_strip, 0, LED_R, LED_G, LED_B);
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

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if ((temp_rc != RCL_RET_OK)) { printf("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); vTaskDelete(NULL); } }

static rcl_subscription_t subscriber;
static std_msgs__msg__Int32 recv_msg;

static void subscription_callback(const void * msgin)
{
    const std_msgs__msg__Int32 * msg = (const std_msgs__msg__Int32 *)msgin;
    printf("Received value: %d\n", (int)msg->data);
    led_set(msg->data != 0);
}

void micro_ros_task(void * arg)
{
    (void)arg;

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    RCCHECK(rcl_init_options_init(&init_options, allocator));

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
    rmw_init_options_t * rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
    RCCHECK(rmw_uros_options_set_udp_address(CONFIG_MICRO_ROS_AGENT_IP, CONFIG_MICRO_ROS_AGENT_PORT, rmw_options));
#endif

    RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));

    rcl_node_t node = rcl_get_zero_initialized_node();
    RCCHECK(rclc_node_init_default(&node, "esp32_minimal_subscriber", "", &support));

    RCCHECK(rclc_subscription_init_default(
        &subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "esp32_rx_int32"));

    rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
    RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
    RCCHECK(rclc_executor_set_timeout(&executor, RCL_MS_TO_NS(10)));
    RCCHECK(rclc_executor_add_subscription(
        &executor,
        &subscriber,
        &recv_msg,
        &subscription_callback,
        ON_NEW_DATA));

    printf("Minimal subscriber ready. Listening on topic: /esp32_rx_int32\n");

    while (1) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
        usleep(1000);
    }

    RCCHECK(rcl_subscription_fini(&subscriber, &node));
    RCCHECK(rcl_node_fini(&node));

    vTaskDelete(NULL);
}

void app_main(void)
{
    led_init();

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
