/* SPDX-License-Identifier: Apache-2.0 */

#ifndef RM_TEST_APP_MODULES_ARM_ARM_MODULE_H_
#define RM_TEST_APP_MODULES_ARM_ARM_MODULE_H_

#include <zephyr/kernel.h>

#include <app/bootstrap/module.h>
#include <app/channels/arm_command_channel.h>
#include <app/channels/arm_state_channel.h>

namespace rm_test::app::modules::arm {

class ArmModule : public bootstrap::Module {
public:
	const char *Name() const override { return "arm"; }
	int Initialize() override;
	int Start() override;

	void ControlClaw(float virtual_angle);
	void ControlElbowPitchJoint(float virtual_angle);
	void ControlElbowYawJoint(float virtual_angle);
	void ControlWristByTwistFlip(float twist_delta, float flip_delta);

private:
	static constexpr float kClawLimit = 0.35f;
	static constexpr float kWristLimit = 1.0f;
	static constexpr float kElbowPitchLimit = 1.0f;
	static constexpr float kElbowYawLimit = 3.14f;

	void RunLoop();
	void HandleCommand(const channels::ArmCommandMessage &command);
	static float Clamp(float value, float lower, float upper);

	struct k_thread thread_;
	bool started_ = false;
	float claws_virtual_angle_ = 0.0f;
	float wrist_joint_left_virtual_angle_ = 0.0f;
	float wrist_joint_right_virtual_angle_ = 0.0f;
	float elbow_pitch_joint_virtual_angle_ = 0.0f;
	float elbow_yaw_joint_virtual_angle_ = 0.0f;
	uint32_t state_sequence_ = 0U;
};

}  // namespace rm_test::app::modules::arm

#endif /* RM_TEST_APP_MODULES_ARM_ARM_MODULE_H_ */
