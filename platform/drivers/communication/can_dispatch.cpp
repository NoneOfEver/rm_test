/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <app/channels/motor_feedback_channel.h>
#include <app/channels/remote_input_channel.h>
#include <app/protocols/mcu_link/mcu_link.h>
#include <app/protocols/motors/cubemars_motor_protocol.h>
#include <app/protocols/motors/dm_motor_protocol.h>
#include <app/protocols/motors/dji_motor_protocol.h>

#include "can_dispatch.h"

namespace {

constexpr int kThreadPrio = 8;
constexpr uint16_t kDjiId1 = 0x201;
constexpr uint16_t kDjiId2 = 0x202;
constexpr uint16_t kDjiId3 = 0x203;
constexpr uint16_t kDjiId4 = 0x204;
constexpr uint16_t kDmId1 = 0x011;
constexpr uint16_t kDmId2 = 0x012;
constexpr uint16_t kDmId3 = 0x013;
constexpr uint16_t kCubemarsBroadcastId = 0x000;
constexpr uint16_t kMcuRemoteControlId = rm_test::app::protocols::mcu_link::kRemoteControlId;

struct CanRxEvent {
	uint16_t can_id;
	uint8_t dlc;
	uint8_t data[8];
};

K_THREAD_STACK_DEFINE(g_can_dispatch_stack, 1024);
K_MSGQ_DEFINE(g_can_rx_msgq, sizeof(CanRxEvent), 16, 4);

struct k_thread g_can_dispatch_thread;
const struct device *g_can_dev = nullptr;
bool g_started = false;
uint32_t g_feedback_sequence = 0U;

void PublishFeedback(const rm_test::app::channels::MotorFeedbackMessage &feedback)
{
	(void)zbus_chan_pub(&rm_test_motor_feedback_chan, &feedback, K_NO_WAIT);
}

void HandleDjiFeedback(const CanRxEvent &event)
{
	if (rm_test::app::protocols::motors::dji::IngestCanFrame(1U, event.can_id, event.dlc, event.data) !=
	    0) {
		return;
	}

	rm_test::app::channels::MotorFeedbackMessage feedback = {};
	if (rm_test::app::protocols::motors::dji::GetLatestState(event.can_id, &feedback) == 0) {
		PublishFeedback(feedback);
	}
}

void HandleDmFeedback(const CanRxEvent &event)
{
	rm_test::app::protocols::motors::dm::DmMotorFeedback1To4 raw = {};
	if (rm_test::app::protocols::motors::dm::DecodeFeedback1To4(event.data, event.dlc, &raw) != 0) {
		return;
	}

	rm_test::app::channels::MotorFeedbackMessage feedback = {};
	feedback.bus = 1U;
	feedback.can_id = event.can_id;
	feedback.encoder = raw.encoder;
	feedback.omega = static_cast<int16_t>(raw.omega_x100 / 100);
	feedback.current = raw.current_ma;
	feedback.temperature = raw.rotor_temperature;
	feedback.sequence = ++g_feedback_sequence;
	PublishFeedback(feedback);
}

void HandleCubemarsFeedback(const CanRxEvent &event)
{
	rm_test::app::protocols::motors::cubemars::CubemarsFeedback raw = {};
	if (rm_test::app::protocols::motors::cubemars::DecodeFeedback(event.data, event.dlc, &raw) != 0) {
		return;
	}

	rm_test::app::channels::MotorFeedbackMessage feedback = {};
	feedback.bus = 1U;
	feedback.can_id = event.can_id;
	feedback.encoder = raw.position_raw;
	feedback.omega = static_cast<int16_t>(raw.velocity_raw);
	feedback.current = static_cast<int16_t>(raw.torque_raw);
	feedback.temperature = 0U;
	feedback.sequence = ++g_feedback_sequence;
	PublishFeedback(feedback);
}

float NormalizeByteAxis(uint8_t raw)
{
	const int centered = static_cast<int>(raw) - 127;
	float v = static_cast<float>(centered) / 127.0f;
	if (v > 1.0f) {
		v = 1.0f;
	}
	if (v < -1.0f) {
		v = -1.0f;
	}
	return v;
}

void HandleMcuRemoteControl(const CanRxEvent &event)
{
	if (rm_test::app::protocols::mcu_link::IngestCanFrame(event.can_id, event.dlc, event.data) != 0) {
		return;
	}

	rm_test::app::protocols::mcu_link::RemoteControlData rc = {};
	if (rm_test::app::protocols::mcu_link::GetLatestRemoteControl(&rc) != 0) {
		return;
	}

	rm_test::app::channels::RemoteInputMessage input = {};
	input.source = rm_test::app::channels::kRemoteInputDr16;
	input.chassis_enable = 1U;
	input.axis_lx = NormalizeByteAxis(rc.chassis_speed_x);
	input.axis_ly = NormalizeByteAxis(rc.chassis_speed_y);
	input.axis_wheel = NormalizeByteAxis(rc.chassis_rotation);
	input.sequence = rc.sequence;
	(void)zbus_chan_pub(&rm_test_remote_input_chan, &input, K_NO_WAIT);
}

void CanRxCallback(const struct device *dev, struct can_frame *frame, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if ((frame->flags & CAN_FRAME_IDE) != 0U) {
		return;
	}

	CanRxEvent event = {};
	event.can_id = static_cast<uint16_t>(frame->id);
	event.dlc = frame->dlc;

	const uint8_t copy_len = (frame->dlc > 8U) ? 8U : frame->dlc;
	for (uint8_t i = 0U; i < copy_len; ++i) {
		event.data[i] = frame->data[i];
	}

	(void)k_msgq_put(&g_can_rx_msgq, &event, K_NO_WAIT);
}

void CanDispatchThreadMain()
{
	printk("can_dispatch started\n");

	while (true) {
		CanRxEvent event = {};
		if (k_msgq_get(&g_can_rx_msgq, &event, K_FOREVER) != 0) {
			continue;
		}

		if ((event.can_id >= kDjiId1) && (event.can_id <= kDjiId4)) {
			HandleDjiFeedback(event);
			continue;
		}

		if ((event.can_id >= kDmId1) && (event.can_id <= kDmId3)) {
			HandleDmFeedback(event);
			continue;
		}

		if (event.can_id == kCubemarsBroadcastId) {
			HandleCubemarsFeedback(event);
			continue;
		}

		if (event.can_id == kMcuRemoteControlId) {
			HandleMcuRemoteControl(event);
		}
	}
}

const struct device *FindCanDevice()
{
#if DT_NODE_HAS_STATUS(DT_NODELABEL(mcan4), okay)
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(mcan4));
	if (device_is_ready(dev)) {
		return dev;
	}
#endif

	const char *const candidates[] = {"CAN_0", "can0", "CAN0"};
	for (size_t i = 0U; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
		const struct device *dev = device_get_binding(candidates[i]);
		if ((dev != nullptr) && device_is_ready(dev)) {
			return dev;
		}
	}

	return nullptr;
}

}  // namespace

namespace rm_test::platform::drivers::communication::can_dispatch {

int Initialize()
{
	if (g_started) {
		return 0;
	}

	g_can_dev = FindCanDevice();
	if (g_can_dev == nullptr) {
		return -ENODEV;
	}

	int rc = can_start(g_can_dev);
	if ((rc != 0) && (rc != -EALREADY)) {
		return rc;
	}

	const struct can_filter filter = {
		.id = 0U,
		.mask = CAN_STD_ID_MASK,
		.flags = 0U,
	};

	struct can_filter f = filter;
	f.id = kDjiId1;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}
	f.id = kDjiId2;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}
	f.id = kDjiId3;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}
	f.id = kDjiId4;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}

	f.id = kDmId1;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}
	f.id = kDmId2;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}
	f.id = kDmId3;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}
	f.id = kCubemarsBroadcastId;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}

	f.id = kMcuRemoteControlId;
	if (can_add_rx_filter(g_can_dev, CanRxCallback, nullptr, &f) < 0) {
		return -EIO;
	}

	k_thread_create(&g_can_dispatch_thread,
			g_can_dispatch_stack,
			K_THREAD_STACK_SIZEOF(g_can_dispatch_stack),
			[](void *p1, void *p2, void *p3) {
				ARG_UNUSED(p1);
				ARG_UNUSED(p2);
				ARG_UNUSED(p3);
				CanDispatchThreadMain();
			},
			nullptr,
			nullptr,
			nullptr,
			K_PRIO_PREEMPT(kThreadPrio),
			0,
			K_NO_WAIT);

	k_thread_name_set(&g_can_dispatch_thread, "can_dispatch");
	g_started = true;
	return 0;
}

int SendStdData(uint16_t can_id, const uint8_t data[8], uint8_t dlc)
{
	if ((data == nullptr) || (dlc > 8U)) {
		return -EINVAL;
	}

	if (g_can_dev == nullptr) {
		return -ENODEV;
	}

	struct can_frame frame = {};
	frame.flags = 0U;
	frame.id = can_id;
	frame.dlc = dlc;
	for (uint8_t i = 0U; i < dlc; ++i) {
		frame.data[i] = data[i];
	}

	return can_send(g_can_dev, &frame, K_NO_WAIT, nullptr, nullptr);
}
}  // namespace rm_test::platform::drivers::communication::can_dispatch
