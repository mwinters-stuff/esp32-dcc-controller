/**
 * @file wifi_connection.cpp
 * @brief Shared FreeRTOS queue used to propagate lwIP TCP errors back to the
 *        WifiControl state machine.
 *
 * `tcp_fail_queue` is written from lwIP callbacks (ISR context) and read by
 * WifiControl::failError() on the wifi_loop_task.
 */
#include "wifi_connection.h"

namespace utilities {
QueueHandle_t tcp_fail_queue = xQueueCreate(10, sizeof(err_t));
} // namespace utilities