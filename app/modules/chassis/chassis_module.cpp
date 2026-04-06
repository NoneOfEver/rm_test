/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>

#include <app/bootstrap/thread_utils.h>
#include <app/modules/chassis/chassis_module.h>
#include <app/protocols/motors/dji_motor_protocol.h>
#include <app/services/actuator/actuator_service.h>
#include <app/services/chassis/chassis_tuning_service.h>

namespace {

K_THREAD_STACK_DEFINE(g_chassis_module_stack, 1024);

}  // namespace

namespace rm_test::app::modules::chassis {

int ChassisModule::Initialize()
{
	state_ = {};
	publish_sequence_ = 0U;
	started_ = false;
	(void)k_mutex_init(&pid_mutex_);

	const int unregister_rc = rm_test::app::services::chassis_tuning::UnregisterProvider(this);
	if ((unregister_rc != 0) && (unregister_rc != -ENOENT)) {
		return unregister_rc;
	}

	const int register_rc =
		rm_test::app::services::chassis_tuning::RegisterProvider(this, "chassis_module", 100);
	if (register_rc != 0) {
		return register_rc;
	}

	return SetSpeedPidTuning(kPidKp, kPidKi, kPidKd, kIntegralLimit, kCurrentLimit);
}

int ChassisModule::SetSpeedPidTuning(float kp, float ki, float kd, float i_limit, float out_limit)
{
	if ((i_limit <= 0.0f) || (out_limit <= 0.0f)) {
		return -EINVAL;
	}

	(void)k_mutex_lock(&pid_mutex_, K_FOREVER);
	pid_kp_ = kp;
	pid_ki_ = ki;
	pid_kd_ = kd;
	pid_i_limit_ = i_limit;
	pid_out_limit_ = out_limit;

	for (int i = 0; i < 4; ++i) {
		wheel_speed_pid_[i].Init(
			pid_kp_,
			pid_ki_,
			pid_kd_,
			0.0f,
			pid_i_limit_,
			pid_out_limit_,
			0.001f);
		wheel_speed_pid_[i].SetIntegralError(0.0f);
	}
	k_mutex_unlock(&pid_mutex_);
	return 0;
}

int ChassisModule::GetSpeedPidTuning(float *kp, float *ki, float *kd, float *i_limit, float *out_limit)
{
	if ((kp == nullptr) || (ki == nullptr) || (kd == nullptr) || (i_limit == nullptr) ||
	    (out_limit == nullptr)) {
		return -EINVAL;
	}

	(void)k_mutex_lock(&pid_mutex_, K_FOREVER);
	*kp = pid_kp_;
	*ki = pid_ki_;
	*kd = pid_kd_;
	*i_limit = pid_i_limit_;
	*out_limit = pid_out_limit_;
	k_mutex_unlock(&pid_mutex_);
	return 0;
}

int ChassisModule::ResetSpeedPidIntegrator()
{
	(void)k_mutex_lock(&pid_mutex_, K_FOREVER);
	for (int i = 0; i < 4; ++i) {
		wheel_speed_pid_[i].reset();
	}
	k_mutex_unlock(&pid_mutex_);
	return 0;
}

int ChassisModule::Start()
{
	if (started_) {
		return 0;
	}

	bootstrap::StartMemberThread<ChassisModule, &ChassisModule::RunLoop>(
		&thread_,
		g_chassis_module_stack,
		K_THREAD_STACK_SIZEOF(g_chassis_module_stack),
		this,
		K_PRIO_PREEMPT(8),
		"chassis_module");
	started_ = true;
	return 0;
}

void ChassisModule::RunLoop()
{
	printk("chassis module started\n");

	channels::ChassisCommandMessage command = {};

	while (true) {
		RefreshMotorFeedbackSnapshot();

		if (zbus_chan_read(&rm_test_chassis_command_chan, &command, K_NO_WAIT) == 0) {
			UpdateStateFromCommand(command);
		}

		ApplyWheelSpeedPidAndSend();

		state_.sequence = ++publish_sequence_;
		(void)zbus_chan_pub(&rm_test_chassis_state_chan, &state_, K_NO_WAIT);
		k_sleep(K_MSEC(1));
	}
}

void ChassisModule::RefreshMotorFeedbackSnapshot()
{
	static constexpr uint16_t kMotorCanId[4] = {0x201, 0x202, 0x203, 0x204};

	for (int i = 0; i < 4; ++i) {
		rm_test::app::channels::MotorFeedbackMessage feedback = {};
		if (rm_test::app::protocols::motors::dji::GetLatestState(
			    kMotorCanId[i],
			    &feedback) == 0) {
			motor_feedback_[i] = feedback;
			motor_feedback_valid_[i] = true;
		}
	}
}

void ChassisModule::UpdateStateFromCommand(const channels::ChassisCommandMessage &command)
{
	const float vx = command.target_vx;
	const float vy = command.target_vy;
	const float wz = command.target_wz;

	state_.wheel1_target_omega = (+kKinematicsFactor * vx - kKinematicsFactor * vy) + wz;
	state_.wheel2_target_omega = (-kKinematicsFactor * vx - kKinematicsFactor * vy) + wz;
	state_.wheel3_target_omega = (-kKinematicsFactor * vx + kKinematicsFactor * vy) + wz;
	state_.wheel4_target_omega = (+kKinematicsFactor * vx + kKinematicsFactor * vy) + wz;
}

void ChassisModule::ApplyWheelSpeedPidAndSend()
{
	const float target[4] = {
		state_.wheel1_target_omega,
		state_.wheel2_target_omega,
		state_.wheel3_target_omega,
		state_.wheel4_target_omega,
	};

	int16_t current_cmd[4] = {0, 0, 0, 0};
	(void)k_mutex_lock(&pid_mutex_, K_FOREVER);
	for (int i = 0; i < 4; ++i) {
		if (!motor_feedback_valid_[i]) {
			continue;
		}

		const float measured = static_cast<float>(motor_feedback_[i].omega);
		const float out = wheel_speed_pid_[i].update(target[i], measured);
		current_cmd[i] = static_cast<int16_t>(out);
	}
	k_mutex_unlock(&pid_mutex_);

	(void)rm_test::app::services::actuator::SendMotorCurrent(
		rm_test::app::services::actuator::MotorCurrentGroup::kDji0x200,
		current_cmd);
}

}  // namespace rm_test::app::modules::chassis
