/* SPDX-License-Identifier: Apache-2.0 */

#ifndef RM_TEST_APP_MODULES_GANTRY_GANTRY_MODULE_H_
#define RM_TEST_APP_MODULES_GANTRY_GANTRY_MODULE_H_

#include <zephyr/kernel.h>

#include <app/bootstrap/module.h>
#include <app/channels/gantry_command_channel.h>

namespace rm_test::app::modules::gantry {

class GantryModule : public bootstrap::Module {
public:
	const char *Name() const override { return "gantry"; }
	int Initialize() override;
	int Start() override;

	void XAxisMoveInDistance(float distance);
	void YAxisMoveInDistance(float distance);
	void ZAxisMoveInDistance(float distance);

private:
	static constexpr float kXAxisLimit = 10.0f;
	static constexpr float kYAxisLimit = 10.0f;
	static constexpr float kZAxisLimit = 5.0f;

	void RunLoop();
	void HandleCommand(const channels::GantryCommandMessage &command);
	static float Clamp(float value, float lower, float upper);

	struct k_thread thread_;
	bool started_ = false;
	float x_axis_virtual_distance_ = 0.0f;
	float y_axis_virtual_distance_ = 0.0f;
	float z_axis_virtual_distance_ = 0.0f;
};

}  // namespace rm_test::app::modules::gantry

#endif /* RM_TEST_APP_MODULES_GANTRY_GANTRY_MODULE_H_ */
