/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/sys/printk.h>

#include <app/bootstrap/thread_utils.h>
#include <app/modules/arm/arm_module.h>

namespace {

K_THREAD_STACK_DEFINE(g_arm_module_stack, 1024);

}  // namespace

namespace rm_test::app::modules::arm {

int ArmModule::Initialize()
{
	started_ = false;
	claws_virtual_angle_ = 0.0f;
	wrist_joint_left_virtual_angle_ = 0.0f;
	wrist_joint_right_virtual_angle_ = 0.0f;
	elbow_pitch_joint_virtual_angle_ = 0.0f;
	elbow_yaw_joint_virtual_angle_ = 0.0f;
	state_sequence_ = 0U;
	return 0;
}

int ArmModule::Start()
{
	if (started_) {
		return 0;
	}

	bootstrap::StartMemberThread<ArmModule, &ArmModule::RunLoop>(&thread_,
							     g_arm_module_stack,
							     K_THREAD_STACK_SIZEOF(g_arm_module_stack),
							     this,
							     K_PRIO_PREEMPT(8),
							     "arm_module");
	started_ = true;
	return 0;
}

float ArmModule::Clamp(float value, float lower, float upper)
{
	if (value < lower) {
		return lower;
	}
	if (value > upper) {
		return upper;
	}
	return value;
}

void ArmModule::ControlClaw(float virtual_angle)
{
	claws_virtual_angle_ = Clamp(virtual_angle, -kClawLimit, kClawLimit);
}

void ArmModule::ControlElbowPitchJoint(float virtual_angle)
{
	elbow_pitch_joint_virtual_angle_ = Clamp(virtual_angle, -kElbowPitchLimit, kElbowPitchLimit);
}

void ArmModule::ControlElbowYawJoint(float virtual_angle)
{
	elbow_yaw_joint_virtual_angle_ = Clamp(virtual_angle, -kElbowYawLimit, kElbowYawLimit);
}

void ArmModule::ControlWristByTwistFlip(float twist_delta, float flip_delta)
{
	wrist_joint_left_virtual_angle_ += flip_delta + twist_delta;
	wrist_joint_right_virtual_angle_ += flip_delta - twist_delta;
	wrist_joint_left_virtual_angle_ =
		Clamp(wrist_joint_left_virtual_angle_, -kWristLimit, kWristLimit);
	wrist_joint_right_virtual_angle_ =
		Clamp(wrist_joint_right_virtual_angle_, -kWristLimit, kWristLimit);
}

void ArmModule::HandleCommand(const channels::ArmCommandMessage &command)
{
	if (command.enable == 0U) {
		return;
	}

	ControlClaw(claws_virtual_angle_ + command.claw_delta);
	ControlElbowYawJoint(elbow_yaw_joint_virtual_angle_ + command.elbow_yaw_delta);
	ControlElbowPitchJoint(elbow_pitch_joint_virtual_angle_ + command.elbow_pitch_delta);
	ControlWristByTwistFlip(command.wrist_twist_delta, command.wrist_flip_delta);
}

void ArmModule::RunLoop()
{
	printk("arm module started\n");

	channels::ArmCommandMessage command = {};
	channels::ArmStateMessage state = {};
	while (true) {
		if (zbus_chan_read(&rm_test_arm_command_chan, &command, K_NO_WAIT) == 0) {
			HandleCommand(command);
		}

		state.claw_virtual_angle = claws_virtual_angle_;
		state.elbow_pitch_virtual_angle = elbow_pitch_joint_virtual_angle_;
		state.elbow_yaw_virtual_angle = elbow_yaw_joint_virtual_angle_;
		state.wrist_left_virtual_angle = wrist_joint_left_virtual_angle_;
		state.wrist_right_virtual_angle = wrist_joint_right_virtual_angle_;
		state.sequence = ++state_sequence_;
		(void)zbus_chan_pub(&rm_test_arm_state_chan, &state, K_NO_WAIT);
		k_sleep(K_MSEC(2));
	}
}

}  // namespace rm_test::app::modules::arm
