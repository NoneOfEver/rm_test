/* SPDX-License-Identifier: Apache-2.0 */

#include <platform/drivers/devices/actuators/serial_servo.h>

#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

namespace rm_test::platform::drivers::devices::actuators::serial_servo {

namespace {

constexpr uint8_t kFrameHead = 0x55;
const struct device *g_uart_dev = nullptr;
bool g_initialized = false;

uint16_t AngleToRaw(float degrees)
{
	float clamped = degrees;
	if (clamped < 0.0f) {
		clamped = 0.0f;
	}
	if (clamped > 240.0f) {
		clamped = 240.0f;
	}

	return static_cast<uint16_t>(clamped * (1000.0f / 240.0f) + 0.5f);
}

int SendPacket(uint8_t id, uint8_t cmd, const uint8_t *params, uint8_t params_len)
{
	if (!g_initialized || (g_uart_dev == nullptr)) {
		return -ENODEV;
	}

	uint8_t frame[16] = {0};
	const uint8_t length = static_cast<uint8_t>(params_len + 3U);
	frame[0] = kFrameHead;
	frame[1] = kFrameHead;
	frame[2] = id;
	frame[3] = length;
	frame[4] = cmd;

	uint16_t sum = static_cast<uint16_t>(id + length + cmd);
	for (uint8_t i = 0U; i < params_len; ++i) {
		frame[5U + i] = params[i];
		sum = static_cast<uint16_t>(sum + params[i]);
	}

	frame[5U + params_len] = static_cast<uint8_t>(~(sum & 0xffU));
	const uint8_t total = static_cast<uint8_t>(6U + params_len);
	for (uint8_t i = 0U; i < total; ++i) {
		uart_poll_out(g_uart_dev, frame[i]);
	}

	return 0;
}

}  // namespace

int Initialize()
{
	if (g_initialized) {
		return 0;
	}

#if DT_HAS_CHOSEN(zephyr_serial_servo_uart)
	g_uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_serial_servo_uart));
#else
	return -ENODEV;
#endif
	if ((g_uart_dev == nullptr) || !device_is_ready(g_uart_dev)) {
		return -ENODEV;
	}

	g_initialized = true;
	return 0;
}

int MoveToAngle(uint8_t id, float degrees, uint16_t time_ms)
{
	const uint16_t raw = AngleToRaw(degrees);
	const uint8_t params[4] = {
		static_cast<uint8_t>(raw & 0xffU),
		static_cast<uint8_t>((raw >> 8) & 0xffU),
		static_cast<uint8_t>(time_ms & 0xffU),
		static_cast<uint8_t>((time_ms >> 8) & 0xffU),
	};
	return SendPacket(id, 1U, params, sizeof(params));
}

int Stop(uint8_t id)
{
	return SendPacket(id, 12U, nullptr, 0U);
}

}  // namespace rm_test::platform::drivers::devices::actuators::serial_servo
