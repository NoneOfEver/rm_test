/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RM_TEST_APP_MODULES_CHASSIS_CHASSIS_MODULE_H_
#define RM_TEST_APP_MODULES_CHASSIS_CHASSIS_MODULE_H_

#include <zephyr/kernel.h>

#include <app/bootstrap/module.h>
#include <app/algorithms/control/alg_pid.h>
#include <app/channels/chassis_command_channel.h>
#include <app/channels/chassis_state_channel.h>
#include <app/channels/motor_feedback_channel.h>
#include <app/services/chassis/chassis_tuning_service.h>

namespace rm_test::app::modules::chassis {

class ChassisModule : public bootstrap::Module,
			  public rm_test::app::services::chassis_tuning::SpeedPidTuningProvider {
public:
	const char *Name() const override { return "chassis"; }
	int Initialize() override;
	int Start() override;

	int SetSpeedPidTuning(float kp, float ki, float kd, float i_limit, float out_limit);
	int GetSpeedPidTuning(float *kp, float *ki, float *kd, float *i_limit, float *out_limit);
	int ResetSpeedPidIntegrator();

private:
	static constexpr float kKinematicsFactor = 0.707107f;
	static constexpr float kPidKp = 120.0f;
	static constexpr float kPidKi = 8.0f;
	static constexpr float kPidKd = 0.0f;
	static constexpr float kIntegralLimit = 500.0f;
	static constexpr float kCurrentLimit = 16384.0f;

	void RunLoop();
	void UpdateStateFromCommand(const channels::ChassisCommandMessage &command);
	void RefreshMotorFeedbackSnapshot();
	void ApplyWheelSpeedPidAndSend();

	struct k_thread thread_;
	bool started_ = false;
	uint32_t publish_sequence_ = 0U;
	channels::ChassisStateMessage state_ = {};
	channels::MotorFeedbackMessage motor_feedback_[4] = {};
	bool motor_feedback_valid_[4] = {false, false, false, false};
	alg::Pid wheel_speed_pid_[4] = {};
	float pid_kp_ = kPidKp;
	float pid_ki_ = kPidKi;
	float pid_kd_ = kPidKd;
	float pid_i_limit_ = kIntegralLimit;
	float pid_out_limit_ = kCurrentLimit;
	struct k_mutex pid_mutex_;
};

}  // namespace rm_test::app::modules::chassis

#endif /* RM_TEST_APP_MODULES_CHASSIS_CHASSIS_MODULE_H_ */
