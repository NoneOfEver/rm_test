/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RM_TEST_APP_MODULES_REMOTE_INPUT_REMOTE_INPUT_MODULE_H_
#define RM_TEST_APP_MODULES_REMOTE_INPUT_REMOTE_INPUT_MODULE_H_

#include <zephyr/kernel.h>

#include <app/bootstrap/module.h>
#include <app/channels/arm_command_channel.h>
#include <app/channels/chassis_command_channel.h>
#include <app/channels/gantry_command_channel.h>
#include <app/channels/gimbal_command_channel.h>
#include <app/channels/remote_input_channel.h>

namespace rm_test::app::modules::remote_input {

class RemoteInputModule : public bootstrap::Module {
public:
	const char *Name() const override { return "remote_input"; }
	int Initialize() override;
	int Start() override;

private:
	static constexpr float kChassisSpeedScale = 20.0f;
	static constexpr float kVt03SpinScale = 0.5f;

	void RunLoop();
	channels::ChassisCommandMessage ComposeChassisCommand(
		const channels::RemoteInputMessage &input) const;
	channels::ArmCommandMessage ComposeArmCommand(const channels::RemoteInputMessage &input) const;
	channels::GimbalCommandMessage ComposeGimbalCommand(
		const channels::RemoteInputMessage &input) const;
	channels::GantryCommandMessage ComposeGantryCommand(
		const channels::RemoteInputMessage &input) const;

	struct k_thread thread_;
	bool started_ = false;
	uint32_t publish_sequence_ = 0U;
};

}  // namespace rm_test::app::modules::remote_input

#endif /* RM_TEST_APP_MODULES_REMOTE_INPUT_REMOTE_INPUT_MODULE_H_ */
