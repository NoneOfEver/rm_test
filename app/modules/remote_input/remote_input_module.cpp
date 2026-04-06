/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>

#include <app/modules/remote_input/remote_input_module.h>
#include <app/bootstrap/thread_utils.h>

namespace {

K_THREAD_STACK_DEFINE(g_remote_input_module_stack, 1024);

}  // namespace

namespace rm_test::app::modules::remote_input {

int RemoteInputModule::Initialize()
{
	publish_sequence_ = 0U;
	started_ = false;
	return 0;
}

int RemoteInputModule::Start()
{
	if (started_) {
		return 0;
	}

	bootstrap::StartMemberThread<RemoteInputModule, &RemoteInputModule::RunLoop>(
		&thread_,
		g_remote_input_module_stack,
		K_THREAD_STACK_SIZEOF(g_remote_input_module_stack),
		this,
		K_PRIO_PREEMPT(8),
		"remote_input_module");

	started_ = true;
	return 0;
}

void RemoteInputModule::RunLoop()
{
	printk("remote_input module started\n");

	channels::RemoteInputMessage input = {};
	channels::ChassisCommandMessage chassis_command = {};
	channels::ArmCommandMessage arm_command = {};
	channels::GimbalCommandMessage gimbal_command = {};
	channels::GantryCommandMessage gantry_command = {};

	while (true) {
		if (zbus_chan_read(&rm_test_remote_input_chan, &input, K_NO_WAIT) == 0) {
			chassis_command = ComposeChassisCommand(input);
			arm_command = ComposeArmCommand(input);
			gimbal_command = ComposeGimbalCommand(input);
			gantry_command = ComposeGantryCommand(input);
		} else {
			chassis_command = {};
			arm_command = {};
			gimbal_command = {};
			gantry_command = {};
			chassis_command.source = channels::kInputSourceUnknown;
			arm_command.source = channels::kInputSourceUnknown;
			gimbal_command.source = channels::kInputSourceUnknown;
			gantry_command.source = channels::kInputSourceUnknown;
		}

		const uint32_t sequence = ++publish_sequence_;
		chassis_command.sequence = sequence;
		arm_command.sequence = sequence;
		gimbal_command.sequence = sequence;
		gantry_command.sequence = sequence;

		(void)zbus_chan_pub(&rm_test_chassis_command_chan, &chassis_command, K_NO_WAIT);
		(void)zbus_chan_pub(&rm_test_arm_command_chan, &arm_command, K_NO_WAIT);
		(void)zbus_chan_pub(&rm_test_gimbal_command_chan, &gimbal_command, K_NO_WAIT);
		(void)zbus_chan_pub(&rm_test_gantry_command_chan, &gantry_command, K_NO_WAIT);
		k_sleep(K_MSEC(1));
	}
}

channels::ChassisCommandMessage RemoteInputModule::ComposeChassisCommand(
	const channels::RemoteInputMessage &input) const
{
	channels::ChassisCommandMessage command = {};

	if (input.chassis_enable == 0U) {
		command.source = channels::kInputSourceUnknown;
		return command;
	}

	switch (input.source) {
	case channels::kRemoteInputDr16:
		command.target_vx = input.axis_lx * kChassisSpeedScale;
		command.target_vy = -input.axis_ly * kChassisSpeedScale;
		command.target_wz = input.axis_wheel * kChassisSpeedScale;
		command.source = channels::kInputSourceDr16;
		break;
	case channels::kRemoteInputVt03:
		command.target_vx = -input.axis_jx * kChassisSpeedScale;
		command.target_vy = input.axis_jy * kChassisSpeedScale;
		command.target_wz = input.axis_jz * kChassisSpeedScale * kVt03SpinScale;
		command.source = channels::kInputSourceVt03;
		break;
	default:
		command.source = channels::kInputSourceUnknown;
		break;
	}

	return command;
}

channels::ArmCommandMessage RemoteInputModule::ComposeArmCommand(
	const channels::RemoteInputMessage &input) const
{
	channels::ArmCommandMessage command = {};

	if (input.chassis_enable == 0U) {
		command.source = channels::kInputSourceUnknown;
		return command;
	}

	switch (input.source) {
	case channels::kRemoteInputDr16:
		command.claw_delta = input.axis_wheel * 0.003f;
		command.elbow_yaw_delta = input.axis_lx * 0.003f;
		command.elbow_pitch_delta = input.axis_ly * 0.003f;
		command.wrist_twist_delta = 0.0f;
		command.wrist_flip_delta = 0.0f;
		command.source = channels::kInputSourceDr16;
		command.enable = 1U;
		break;
	case channels::kRemoteInputVt03:
		command.claw_delta = 0.0f;
		command.elbow_yaw_delta = input.axis_jx * 0.003f;
		command.elbow_pitch_delta = 0.0f;
		command.wrist_twist_delta = input.axis_jz * 0.003f;
		command.wrist_flip_delta = input.axis_jy * 0.003f;
		command.source = channels::kInputSourceVt03;
		command.enable = 1U;
		break;
	default:
		command.source = channels::kInputSourceUnknown;
		break;
	}

	return command;
}

channels::GimbalCommandMessage RemoteInputModule::ComposeGimbalCommand(
	const channels::RemoteInputMessage &input) const
{
	channels::GimbalCommandMessage command = {};

	if ((input.chassis_enable == 0U) || (input.source != channels::kRemoteInputVt03)) {
		command.source = channels::kInputSourceUnknown;
		return command;
	}

	command.yaw_delta_deg = input.axis_jx * 1.2f;
	command.pitch_delta_deg = input.axis_jy * 0.8f;
	command.source = channels::kInputSourceVt03;
	command.enable = 1U;
	return command;
}

channels::GantryCommandMessage RemoteInputModule::ComposeGantryCommand(
	const channels::RemoteInputMessage &input) const
{
	channels::GantryCommandMessage command = {};

	if (input.chassis_enable == 0U) {
		command.source = channels::kInputSourceUnknown;
		return command;
	}

	switch (input.source) {
	case channels::kRemoteInputDr16:
		command.x_delta = input.axis_lx * 0.02f;
		command.y_delta = input.axis_ly * 0.02f;
		command.z_delta = input.axis_wheel * 0.01f;
		command.source = channels::kInputSourceDr16;
		command.enable = 1U;
		break;
	case channels::kRemoteInputVt03:
		command.x_delta = input.axis_jx * 0.02f;
		command.y_delta = input.axis_jy * 0.02f;
		command.z_delta = input.axis_jz * 0.01f;
		command.source = channels::kInputSourceVt03;
		command.enable = 1U;
		break;
	default:
		command.source = channels::kInputSourceUnknown;
		break;
	}

	return command;
}

}  // namespace rm_test::app::modules::remote_input
