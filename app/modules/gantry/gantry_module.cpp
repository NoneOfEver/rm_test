/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/sys/printk.h>

#include <app/bootstrap/thread_utils.h>
#include <app/modules/gantry/gantry_module.h>

namespace {

K_THREAD_STACK_DEFINE(g_gantry_module_stack, 1024);

}  // namespace

namespace rm_test::app::modules::gantry {

int GantryModule::Initialize()
{
	started_ = false;
	x_axis_virtual_distance_ = 0.0f;
	y_axis_virtual_distance_ = 0.0f;
	z_axis_virtual_distance_ = 0.0f;
	return 0;
}

int GantryModule::Start()
{
	if (started_) {
		return 0;
	}

	bootstrap::StartMemberThread<GantryModule, &GantryModule::RunLoop>(
		&thread_,
		g_gantry_module_stack,
		K_THREAD_STACK_SIZEOF(g_gantry_module_stack),
		this,
		K_PRIO_PREEMPT(8),
		"gantry_module");
	started_ = true;
	return 0;
}

float GantryModule::Clamp(float value, float lower, float upper)
{
	if (value < lower) {
		return lower;
	}
	if (value > upper) {
		return upper;
	}
	return value;
}

void GantryModule::XAxisMoveInDistance(float distance)
{
	x_axis_virtual_distance_ = Clamp(distance, -kXAxisLimit, kXAxisLimit);
}

void GantryModule::YAxisMoveInDistance(float distance)
{
	y_axis_virtual_distance_ = Clamp(distance, -kYAxisLimit, kYAxisLimit);
}

void GantryModule::ZAxisMoveInDistance(float distance)
{
	z_axis_virtual_distance_ = Clamp(distance, -kZAxisLimit, kZAxisLimit);
}

void GantryModule::HandleCommand(const channels::GantryCommandMessage &command)
{
	if (command.enable == 0U) {
		return;
	}

	XAxisMoveInDistance(x_axis_virtual_distance_ + command.x_delta);
	YAxisMoveInDistance(y_axis_virtual_distance_ + command.y_delta);
	ZAxisMoveInDistance(z_axis_virtual_distance_ + command.z_delta);
}

void GantryModule::RunLoop()
{
	printk("gantry module started\n");

	channels::GantryCommandMessage command = {};
	while (true) {
		if (zbus_chan_read(&rm_test_gantry_command_chan, &command, K_NO_WAIT) == 0) {
			HandleCommand(command);
		}
		k_sleep(K_MSEC(2));
	}
}

}  // namespace rm_test::app::modules::gantry
