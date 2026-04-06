/* SPDX-License-Identifier: Apache-2.0 */

#include <app/protocols/mcu_link/mcu_link.h>

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

namespace rm_test::app::protocols::mcu_link {

namespace {

RemoteControlData g_remote = {127U, 127U, 127U, 127U, 127U, kSpinDisable, 0U, 0U};
AutoAimInfoData g_auto_aim = {};
ImuInfoData g_imu = {};
struct k_mutex g_mutex;
bool g_initialized = false;
uint32_t g_sequence = 0U;

void EnsureInitialized()
{
	if (g_initialized) {
		return;
	}

	(void)k_mutex_init(&g_mutex);
	g_initialized = true;
}

}  // namespace

int DecodeRemoteControl(const uint8_t *data, uint8_t dlc, RemoteControlData *out)
{
	if ((data == nullptr) || (out == nullptr) || (dlc < 7U)) {
		return -EINVAL;
	}

	ChassisSpinMode spin = kSpinDisable;
	switch (data[5]) {
	case 0U:
		spin = kSpinClockwise;
		break;
	case 2U:
		spin = kSpinCounterClockwise;
		break;
	default:
		spin = kSpinDisable;
		break;
	}

	out->yaw = data[0];
	out->pitch_angle = data[1];
	out->chassis_speed_x = data[2];
	out->chassis_speed_y = data[3];
	out->chassis_rotation = data[4];
	out->chassis_spin = spin;
	out->supercap = data[6];
	out->sequence = 0U;
	return 0;
}

int DecodeAutoAimInfo(const uint8_t *data, size_t len, AutoAimInfoData *out)
{
	if ((data == nullptr) || (out == nullptr) || (len < 24U)) {
		return -EINVAL;
	}

	memcpy(&out->yaw_angle, &data[0], sizeof(float));
	memcpy(&out->pitch_angle, &data[4], sizeof(float));
	memcpy(&out->yaw_omega, &data[8], sizeof(float));
	memcpy(&out->pitch_omega, &data[12], sizeof(float));
	memcpy(&out->yaw_torque, &data[16], sizeof(float));
	memcpy(&out->pitch_torque, &data[20], sizeof(float));
	out->sequence = 0U;
	return 0;
}

int DecodeImuInfo(const uint8_t *data, size_t len, ImuInfoData *out)
{
	if ((data == nullptr) || (out == nullptr) || (len < 12U)) {
		return -EINVAL;
	}

	memcpy(&out->yaw_total_angle, &data[0], sizeof(float));
	memcpy(&out->pitch, &data[4], sizeof(float));
	memcpy(&out->yaw_omega, &data[8], sizeof(float));
	out->sequence = 0U;
	return 0;
}

int EncodeGimbalInfo(const GimbalInfoData *in, uint8_t out[16])
{
	if ((in == nullptr) || (out == nullptr)) {
		return -EINVAL;
	}

	memcpy(&out[0], &in->yaw_angle, sizeof(float));
	memcpy(&out[4], &in->yaw_omega, sizeof(float));
	memcpy(&out[8], &in->pitch_angle, sizeof(float));
	memcpy(&out[12], &in->pitch_omega, sizeof(float));
	return 0;
}

int IngestCanFrame(uint16_t can_id, uint8_t dlc, const uint8_t *data)
{
	if (data == nullptr) {
		return -EINVAL;
	}

	EnsureInitialized();

	if (can_id == kRemoteControlId) {
		RemoteControlData decoded = {};
		const int rc = DecodeRemoteControl(data, dlc, &decoded);
		if (rc != 0) {
			return rc;
		}

		(void)k_mutex_lock(&g_mutex, K_FOREVER);
		decoded.sequence = ++g_sequence;
		g_remote = decoded;
		k_mutex_unlock(&g_mutex);
		return 0;
	}

	return -ENOTSUP;
}

int GetLatestRemoteControl(RemoteControlData *out)
{
	if (out == nullptr) {
		return -EINVAL;
	}

	EnsureInitialized();
	(void)k_mutex_lock(&g_mutex, K_FOREVER);
	*out = g_remote;
	k_mutex_unlock(&g_mutex);
	return 0;
}

int GetLatestAutoAimInfo(AutoAimInfoData *out)
{
	if (out == nullptr) {
		return -EINVAL;
	}

	EnsureInitialized();
	(void)k_mutex_lock(&g_mutex, K_FOREVER);
	*out = g_auto_aim;
	k_mutex_unlock(&g_mutex);
	return 0;
}

int GetLatestImuInfo(ImuInfoData *out)
{
	if (out == nullptr) {
		return -EINVAL;
	}

	EnsureInitialized();
	(void)k_mutex_lock(&g_mutex, K_FOREVER);
	*out = g_imu;
	k_mutex_unlock(&g_mutex);
	return 0;
}

}  // namespace rm_test::app::protocols::mcu_link
