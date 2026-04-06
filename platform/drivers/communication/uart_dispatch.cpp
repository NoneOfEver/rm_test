/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <app/channels/remote_input_channel.h>
#include <app/protocols/remote_input/dr16_protocol.h>
#include <app/protocols/remote_input/vt03_protocol.h>
#include <platform/drivers/devices/system/referee_client.h>

#include "uart_dispatch.h"

namespace {

#if DT_HAS_CHOSEN(zephyr_uart_mcumgr)
#define RM_TEST_UART_DISPATCH_NODE DT_CHOSEN(zephyr_uart_mcumgr)
#else
#define RM_TEST_UART_DISPATCH_NODE DT_CHOSEN(zephyr_shell_uart)
#endif

constexpr size_t kLineBufSize = 96;
#if defined(CONFIG_UART_ASYNC_API) && CONFIG_UART_ASYNC_API
constexpr size_t kUartRxBufSize = 128;
constexpr size_t kRxChunkSize = 32;
#endif
constexpr size_t kBinaryBufSize = 256;
constexpr int kThreadPrio = 8;
#if defined(CONFIG_UART_ASYNC_API) && CONFIG_UART_ASYNC_API
constexpr int32_t kRxIdleTimeoutUs = 1000;
#endif

K_THREAD_STACK_DEFINE(g_uart_dispatch_stack, 1024);

#if defined(CONFIG_UART_ASYNC_API) && CONFIG_UART_ASYNC_API
struct UartRxChunk {
	uint8_t len;
	uint8_t data[kRxChunkSize];
};

K_MSGQ_DEFINE(g_uart_rx_msgq, sizeof(UartRxChunk), 32, 4);
#endif

struct k_thread g_uart_dispatch_thread;
const struct device *g_uart_dev = nullptr;
bool g_started = false;
uint32_t g_sequence = 0U;
uint8_t g_binary_buf[kBinaryBufSize];
size_t g_binary_len = 0U;

#if defined(CONFIG_UART_ASYNC_API) && CONFIG_UART_ASYNC_API
uint8_t g_uart_rx_buf_a[kUartRxBufSize];
uint8_t g_uart_rx_buf_b[kUartRxBufSize];
#endif

int ParseLine(const char *line, rm_test::app::channels::RemoteInputMessage *out)
{
	if (line == nullptr || out == nullptr) {
		return -EINVAL;
	}

	char type[8] = {0};
	float a = 0.0f;
	float b = 0.0f;
	float c = 0.0f;
	int en = 0;

	if (sscanf(line, "%7s %f %f %f %d", type, &a, &b, &c, &en) != 5) {
		return -EINVAL;
	}

	if (strcmp(type, "dr16") == 0) {
		out->source = rm_test::app::channels::kRemoteInputDr16;
		out->axis_lx = a;
		out->axis_ly = b;
		out->axis_wheel = c;
		out->axis_jx = 0.0f;
		out->axis_jy = 0.0f;
		out->axis_jz = 0.0f;
	} else if (strcmp(type, "vt03") == 0) {
		out->source = rm_test::app::channels::kRemoteInputVt03;
		out->axis_jx = a;
		out->axis_jy = b;
		out->axis_jz = c;
		out->axis_lx = 0.0f;
		out->axis_ly = 0.0f;
		out->axis_wheel = 0.0f;
	} else {
		return -EINVAL;
	}

	out->chassis_enable = (en != 0) ? 1U : 0U;
	out->sequence = ++g_sequence;
	return 0;
}

void ConsumeBinary(size_t bytes)
{
	if ((bytes == 0U) || (g_binary_len == 0U)) {
		return;
	}

	if (bytes >= g_binary_len) {
		g_binary_len = 0U;
		return;
	}

	memmove(g_binary_buf, g_binary_buf + bytes, g_binary_len - bytes);
	g_binary_len -= bytes;
}

void PublishRemoteInput(const rm_test::app::channels::RemoteInputMessage &input)
{
	(void)zbus_chan_pub(&rm_test_remote_input_chan, &input, K_NO_WAIT);
}

void TryDecodeBinaryFrames()
{
	while (g_binary_len > 0U) {
		rm_test::app::channels::RemoteInputMessage input = {};

		if ((g_binary_len >= rm_test::app::protocols::remote_input::vt03::kRemoteFrameLength) &&
		    (g_binary_buf[0] == 0xa9U) && (g_binary_buf[1] == 0x53U)) {
			rm_test::app::protocols::remote_input::vt03::Vt03Frame vt03_frame = {};
			if (rm_test::app::protocols::remote_input::vt03::DecodeRemoteFrame(
				    g_binary_buf,
				    g_binary_len,
				    &vt03_frame)) {
				input.source = rm_test::app::channels::kRemoteInputVt03;
				input.axis_jx = vt03_frame.left_y;
				input.axis_jy = vt03_frame.left_x;
				input.axis_jz = vt03_frame.wheel;
				input.chassis_enable = vt03_frame.chassis_enable ? 1U : 0U;
				input.sequence = ++g_sequence;
				PublishRemoteInput(input);
				ConsumeBinary(rm_test::app::protocols::remote_input::vt03::kRemoteFrameLength);
				continue;
			}
			ConsumeBinary(1U);
			continue;
		}

		if ((g_binary_len >= rm_test::app::protocols::remote_input::vt03::kCustomFrameLength) &&
		    (g_binary_buf[0] == 0xa5U)) {
			rm_test::app::protocols::remote_input::vt03::Vt03CustomFrame custom = {};
			if (rm_test::app::protocols::remote_input::vt03::DecodeCustomFrame(
				    g_binary_buf,
				    g_binary_len,
				    &custom)) {
				input.source = rm_test::app::channels::kRemoteInputVt03;
				input.axis_jx = custom.joystick_x;
				input.axis_jy = custom.joystick_y;
				input.axis_jz = custom.joystick_z;
				input.chassis_enable = custom.chassis_enable ? 1U : 0U;
				input.sequence = ++g_sequence;
				PublishRemoteInput(input);
				ConsumeBinary(rm_test::app::protocols::remote_input::vt03::kCustomFrameLength);
				continue;
			}
		}

		if (g_binary_len >= rm_test::app::protocols::remote_input::dr16::kFrameLength) {
			rm_test::app::protocols::remote_input::dr16::Dr16Frame dr16_frame = {};
			if (rm_test::app::protocols::remote_input::dr16::DecodeFrame(
				    g_binary_buf,
				    g_binary_len,
				    &dr16_frame)) {
				input.source = rm_test::app::channels::kRemoteInputDr16;
				input.axis_lx = dr16_frame.left_stick_x;
				input.axis_ly = dr16_frame.left_stick_y;
				input.axis_wheel = dr16_frame.wheel;
				input.chassis_enable = dr16_frame.chassis_enable ? 1U : 0U;
				input.sequence = ++g_sequence;
				PublishRemoteInput(input);
				ConsumeBinary(rm_test::app::protocols::remote_input::dr16::kFrameLength);
				continue;
			}
		}

		if (g_binary_len < rm_test::app::protocols::remote_input::dr16::kFrameLength) {
			break;
		}

		ConsumeBinary(1U);
	}
}

void FeedBinaryByte(uint8_t byte)
{
	(void)rm_test::platform::drivers::devices::system::referee_client::FeedBytes(&byte, 1U);

	if (g_binary_len < sizeof(g_binary_buf)) {
		g_binary_buf[g_binary_len++] = byte;
	} else {
		memmove(g_binary_buf, g_binary_buf + 1, sizeof(g_binary_buf) - 1U);
		g_binary_buf[sizeof(g_binary_buf) - 1U] = byte;
	}

	TryDecodeBinaryFrames();
}

void ConsumeRxByte(unsigned char ch, char *line, size_t *pos)
{
	if (ch == '\r') {
		return;
	}

	if (ch == '\n') {
		line[*pos] = '\0';
		if (*pos > 0U) {
			rm_test::app::channels::RemoteInputMessage input = {};
			if (ParseLine(line, &input) == 0) {
				(void)zbus_chan_pub(&rm_test_remote_input_chan, &input, K_NO_WAIT);
			}
		}
		*pos = 0U;
		line[0] = '\0';
		return;
	}

	if (isprint(ch) == 0) {
		return;
	}

	if (*pos < (kLineBufSize - 1U)) {
		line[(*pos)++] = static_cast<char>(ch);
	} else {
		*pos = 0U;
		line[0] = '\0';
	}
}

#if defined(CONFIG_UART_ASYNC_API) && CONFIG_UART_ASYNC_API
void UartRxCallback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	switch (evt->type) {
	case UART_RX_RDY: {
		const uint8_t *src = evt->data.rx.buf + evt->data.rx.offset;
		size_t remain = evt->data.rx.len;

		while (remain > 0U) {
			UartRxChunk chunk = {};
			const size_t step = (remain > kRxChunkSize) ? kRxChunkSize : remain;
			chunk.len = static_cast<uint8_t>(step);
			memcpy(chunk.data, src, step);
			(void)k_msgq_put(&g_uart_rx_msgq, &chunk, K_NO_WAIT);

			src += step;
			remain -= step;
		}
		break;
	}
	case UART_RX_BUF_REQUEST: {
		uint8_t *next_buf = (evt->data.rx_buf.buf == g_uart_rx_buf_a)
					 ? g_uart_rx_buf_b
					 : g_uart_rx_buf_a;
		(void)uart_rx_buf_rsp(g_uart_dev, next_buf, sizeof(g_uart_rx_buf_a));
		break;
	}
	case UART_RX_DISABLED:
		(void)uart_rx_enable(g_uart_dev,
				    g_uart_rx_buf_a,
				    sizeof(g_uart_rx_buf_a),
				    kRxIdleTimeoutUs);
		break;
	default:
		break;
	}
}
#endif

void UartDispatchThreadMain()
{
	printk("uart_dispatch started\n");

	char line[kLineBufSize] = {0};
	size_t pos = 0U;

	while (true) {
#if defined(CONFIG_UART_ASYNC_API) && CONFIG_UART_ASYNC_API
		UartRxChunk chunk = {};
		if (k_msgq_get(&g_uart_rx_msgq, &chunk, K_FOREVER) != 0) {
			continue;
		}

		for (uint8_t i = 0U; i < chunk.len; ++i) {
			FeedBinaryByte(chunk.data[i]);
			ConsumeRxByte(chunk.data[i], line, &pos);
		}
#else
		uint8_t ch = 0U;
		if (uart_poll_in(g_uart_dev, &ch) == 0) {
			FeedBinaryByte(ch);
			ConsumeRxByte(ch, line, &pos);
		} else {
			k_sleep(K_MSEC(1));
		}
#endif
	}
}

}  // namespace

namespace rm_test::platform::drivers::communication::uart_dispatch {

int Initialize()
{
	if (g_started) {
		return 0;
	}

#if DT_SAME_NODE(RM_TEST_UART_DISPATCH_NODE, DT_CHOSEN(zephyr_console))
	/* Avoid attaching async RX callback on the same UART used by printk/console. */
	printk("uart_dispatch skipped: dispatch uart is console\n");
	return 0;
#endif

	g_uart_dev = DEVICE_DT_GET(RM_TEST_UART_DISPATCH_NODE);
	if ((g_uart_dev == nullptr) || !device_is_ready(g_uart_dev)) {
		return -ENODEV;
	}

#if defined(CONFIG_UART_ASYNC_API) && CONFIG_UART_ASYNC_API
	if (uart_callback_set(g_uart_dev, UartRxCallback, nullptr) != 0) {
		return -ENOTSUP;
	}

	if (uart_rx_enable(g_uart_dev,
			   g_uart_rx_buf_a,
			   sizeof(g_uart_rx_buf_a),
			   kRxIdleTimeoutUs) != 0) {
		return -EIO;
	}
#endif

	k_thread_create(&g_uart_dispatch_thread,
			g_uart_dispatch_stack,
			K_THREAD_STACK_SIZEOF(g_uart_dispatch_stack),
			[](void *p1, void *p2, void *p3) {
				ARG_UNUSED(p1);
				ARG_UNUSED(p2);
				ARG_UNUSED(p3);
				UartDispatchThreadMain();
			},
			nullptr,
			nullptr,
			nullptr,
			K_PRIO_PREEMPT(kThreadPrio),
			0,
			K_NO_WAIT);

	k_thread_name_set(&g_uart_dispatch_thread, "uart_dispatch");
	g_started = true;
	return 0;
}

}  // namespace rm_test::platform::drivers::communication::uart_dispatch
