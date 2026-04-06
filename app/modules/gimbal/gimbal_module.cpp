/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/sys/printk.h>

#include <app/bootstrap/thread_utils.h>
#include <app/modules/gimbal/gimbal_module.h>
#include <platform/drivers/devices/actuators/serial_servo.h>

namespace {

K_THREAD_STACK_DEFINE(g_gimbal_module_stack, 1024);

}  // namespace

namespace rm_test::app::modules::gimbal {

int GimbalModule::Initialize()
{
	started_ = false;
	servo_ready_ =
		(rm_test::platform::drivers::devices::actuators::serial_servo::Initialize() == 0);
	yaw_angle_deg_ = 0.0f;
	pitch_angle_deg_ = 0.0f;
	state_sequence_ = 0U;
	return 0;
}

int GimbalModule::Start()
{
	if (started_) {
		return 0;
	}

	bootstrap::StartMemberThread<GimbalModule, &GimbalModule::RunLoop>(
		&thread_,
		g_gimbal_module_stack,
		K_THREAD_STACK_SIZEOF(g_gimbal_module_stack),
		this,
		K_PRIO_PREEMPT(8),
		"gimbal_module");
	started_ = true;
	return 0;
}

float GimbalModule::Clamp(float value, float lower, float upper)
{
	if (value < lower) {
		return lower;
	}
	if (value > upper) {
		return upper;
	}
	return value;
}

void GimbalModule::SetYawAngle(float degrees)
{
	yaw_angle_deg_ = Clamp(degrees, kYawMin, kYawMax);
}

void GimbalModule::SetPitchAngle(float degrees)
{
	pitch_angle_deg_ = Clamp(degrees, kPitchMin, kPitchMax);
}

void GimbalModule::HandleCommand(const channels::GimbalCommandMessage &command)
{
	if (command.enable == 0U) {
		return;
	}

	SetYawAngle(yaw_angle_deg_ + command.yaw_delta_deg);
	SetPitchAngle(pitch_angle_deg_ + command.pitch_delta_deg);
}

void GimbalModule::RunLoop()
{
	printk("gimbal module started (servo_ready=%d)\n", servo_ready_ ? 1 : 0);

	channels::GimbalCommandMessage command = {};
	channels::GimbalStateMessage state = {};
	while (true) {
		if (zbus_chan_read(&rm_test_gimbal_command_chan, &command, K_NO_WAIT) == 0) {
			HandleCommand(command);
		}

		if (servo_ready_) {
			const float yaw_servo_angle = yaw_angle_deg_ + 120.0f;
			const float pitch_servo_angle = pitch_angle_deg_ + 90.0f;
			(void)rm_test::platform::drivers::devices::actuators::serial_servo::MoveToAngle(
				kYawServoId,
				yaw_servo_angle,
				kServoMoveTimeMs);
			(void)rm_test::platform::drivers::devices::actuators::serial_servo::MoveToAngle(
				kPitchServoId,
				pitch_servo_angle,
				kServoMoveTimeMs);
		}

		state.yaw_angle_deg = yaw_angle_deg_;
		state.pitch_angle_deg = pitch_angle_deg_;
		state.servo_ready = servo_ready_ ? 1U : 0U;
		state.sequence = ++state_sequence_;
		(void)zbus_chan_pub(&rm_test_gimbal_state_chan, &state, K_NO_WAIT);
		k_sleep(K_MSEC(2));
	}
}

}  // namespace rm_test::app::modules::gimbal
