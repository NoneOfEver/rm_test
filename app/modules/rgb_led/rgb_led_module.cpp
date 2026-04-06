/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>

#include <app/bootstrap/thread_utils.h>
#include <app/modules/rgb_led/rgb_led_module.h>

namespace {

#if DT_NODE_EXISTS(DT_ALIAS(led0))
const gpio_dt_spec kLedR = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led1))
const gpio_dt_spec kLedG = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led2))
const gpio_dt_spec kLedB = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
#endif

K_THREAD_STACK_DEFINE(g_rgb_led_module_stack, 1024);

constexpr uint8_t kPwmLevels = 64U;
constexpr uint32_t kPwmTickUs = 250U;
constexpr uint16_t kBreathSteps = 200U;

uint8_t TriangleBrightnessPercent(uint16_t step)
{
	const uint16_t half = kBreathSteps / 2U;
	if (step < half) {
		return static_cast<uint8_t>((step * 100U) / half);
	}

	return static_cast<uint8_t>(((kBreathSteps - step) * 100U) / half);
}

uint8_t PercentToDuty(uint8_t pct)
{
	if (pct >= 100U) {
		return static_cast<uint8_t>(kPwmLevels - 1U);
	}

	return static_cast<uint8_t>((pct * kPwmLevels) / 100U);
}

}  // namespace

namespace rm_test::app::modules::rgb_led {

volatile uint32_t g_rgb_diag_state = kRgbDiagBoot;

int RgbLedModule::Initialize()
{
	g_rgb_diag_state = kRgbDiagInitEnter;
	started_ = false;
	ready_ = false;

#if DT_NODE_EXISTS(DT_ALIAS(led0)) && DT_NODE_EXISTS(DT_ALIAS(led1)) && DT_NODE_EXISTS(DT_ALIAS(led2))
	led_r_ = kLedR;
	led_g_ = kLedG;
	led_b_ = kLedB;

	if (!gpio_is_ready_dt(&led_r_) || !gpio_is_ready_dt(&led_g_) || !gpio_is_ready_dt(&led_b_)) {
		g_rgb_diag_state = kRgbDiagInitGpioNotReady;
		printk("rgb_led init skipped: gpio not ready\n");
		return 0;
	}

	int rc = gpio_pin_configure_dt(&led_r_, GPIO_OUTPUT_INACTIVE);
	if (rc != 0) {
		g_rgb_diag_state = kRgbDiagInitConfigRFail;
		printk("rgb_led init skipped: led_r config failed (%d)\n", rc);
		return 0;
	}
	rc = gpio_pin_configure_dt(&led_g_, GPIO_OUTPUT_INACTIVE);
	if (rc != 0) {
		g_rgb_diag_state = kRgbDiagInitConfigGFail;
		printk("rgb_led init skipped: led_g config failed (%d)\n", rc);
		return 0;
	}
	rc = gpio_pin_configure_dt(&led_b_, GPIO_OUTPUT_INACTIVE);
	if (rc != 0) {
		g_rgb_diag_state = kRgbDiagInitConfigBFail;
		printk("rgb_led init skipped: led_b config failed (%d)\n", rc);
		return 0;
	}

	ready_ = true;
	g_rgb_diag_state = kRgbDiagInitReady;
	return 0;
#else
	g_rgb_diag_state = kRgbDiagInitNoAliases;
	printk("rgb_led init skipped: led aliases not found\n");
	return 0;
#endif
}

int RgbLedModule::Start()
{
	if (!ready_) {
		g_rgb_diag_state = kRgbDiagStartSkippedNotReady;
		return 0;
	}

	if (started_) {
		g_rgb_diag_state = kRgbDiagStartSkippedAlreadyStarted;
		return 0;
	}

	k_tid_t tid = bootstrap::StartMemberThread<RgbLedModule, &RgbLedModule::RunLoop>(
		&thread_,
		g_rgb_led_module_stack,
		K_THREAD_STACK_SIZEOF(g_rgb_led_module_stack),
		this,
		K_PRIO_PREEMPT(12),
		"rgb_led_module");

	if (tid == nullptr) {
		g_rgb_diag_state = kRgbDiagStartThreadCreateFail;
		return -ENOMEM;
	}

	started_ = true;
	g_rgb_diag_state = kRgbDiagStartThreadCreated;
	return 0;
}

void RgbLedModule::ApplyDuty(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t pwm_counter = 0U;
	while (pwm_counter < kPwmLevels) {
		(void)gpio_pin_set_dt(&led_r_, (pwm_counter < r) ? 1 : 0);
		(void)gpio_pin_set_dt(&led_g_, (pwm_counter < g) ? 1 : 0);
		(void)gpio_pin_set_dt(&led_b_, (pwm_counter < b) ? 1 : 0);

		++pwm_counter;
		k_busy_wait(kPwmTickUs);
	}
}

void RgbLedModule::RunLoop()
{
	g_rgb_diag_state = kRgbDiagRunLoopEnter;
	printk("rgb_led module started\n");

	uint16_t breathe_step = 0U;
	uint8_t color_idx = 0U;

	while (true) {
		const uint8_t brightness_pct = TriangleBrightnessPercent(breathe_step);
		const uint8_t duty = PercentToDuty(brightness_pct);

		switch (color_idx) {
		case 0U:
			ApplyDuty(duty, 0U, 0U);
			break;
		case 1U:
			ApplyDuty(0U, duty, 0U);
			break;
		default:
			ApplyDuty(0U, 0U, duty);
			break;
		}

		++breathe_step;
		if (breathe_step >= kBreathSteps) {
			breathe_step = 0U;
			color_idx = static_cast<uint8_t>((color_idx + 1U) % 3U);
		}

        k_sleep(K_MSEC(1));

	}
}

}  // namespace rm_test::app::modules::rgb_led
