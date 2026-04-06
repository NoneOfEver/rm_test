/* SPDX-License-Identifier: Apache-2.0 */

#ifndef RM_TEST_APP_MODULES_RGB_LED_RGB_LED_MODULE_H_
#define RM_TEST_APP_MODULES_RGB_LED_RGB_LED_MODULE_H_

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <app/bootstrap/module.h>

namespace rm_test::app::modules::rgb_led {

enum : uint32_t {
	kRgbDiagBoot = 0U,
	kRgbDiagInitEnter = 1U,
	kRgbDiagInitGpioNotReady = 2U,
	kRgbDiagInitConfigRFail = 3U,
	kRgbDiagInitConfigGFail = 4U,
	kRgbDiagInitConfigBFail = 5U,
	kRgbDiagInitNoAliases = 6U,
	kRgbDiagInitReady = 7U,
	kRgbDiagStartSkippedNotReady = 8U,
	kRgbDiagStartSkippedAlreadyStarted = 9U,
	kRgbDiagStartThreadCreateFail = 10U,
	kRgbDiagStartThreadCreated = 11U,
	kRgbDiagRunLoopEnter = 12U,
};

extern volatile uint32_t g_rgb_diag_state;

class RgbLedModule : public bootstrap::Module {
public:
	const char *Name() const override { return "rgb_led"; }
	int Initialize() override;
	int Start() override;

private:
	void RunLoop();
	void ApplyDuty(uint8_t r, uint8_t g, uint8_t b);

	struct gpio_dt_spec led_r_;
	struct gpio_dt_spec led_g_;
	struct gpio_dt_spec led_b_;

	struct k_thread thread_;
	bool started_ = false;
	bool ready_ = false;
};

}  // namespace rm_test::app::modules::rgb_led

#endif /* RM_TEST_APP_MODULES_RGB_LED_RGB_LED_MODULE_H_ */
