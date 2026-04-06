/* SPDX-License-Identifier: Apache-2.0 */

#ifndef RM_TEST_APP_PROTOCOLS_MCU_LINK_MCU_LINK_H_
#define RM_TEST_APP_PROTOCOLS_MCU_LINK_MCU_LINK_H_

#include <stddef.h>
#include <stdint.h>

namespace rm_test::app::protocols::mcu_link {

constexpr uint16_t kRemoteControlId = 0x00abU;
constexpr uint16_t kGimbalInfoId = 0x000aU;
constexpr uint16_t kAutoAimInfoId = 0x00faU;
constexpr uint16_t kImuInfoId = 0x00aeU;

enum ChassisSpinMode : uint8_t {
	kSpinClockwise = 0,
	kSpinDisable = 1,
	kSpinCounterClockwise = 2,
};

struct RemoteControlData {
	uint8_t yaw;
	uint8_t pitch_angle;
	uint8_t chassis_speed_x;
	uint8_t chassis_speed_y;
	uint8_t chassis_rotation;
	ChassisSpinMode chassis_spin;
	uint8_t supercap;
	uint32_t sequence;
};

struct GimbalInfoData {
	float yaw_angle;
	float yaw_omega;
	float pitch_angle;
	float pitch_omega;
};

struct AutoAimInfoData {
	float yaw_angle;
	float yaw_omega;
	float yaw_torque;
	float pitch_angle;
	float pitch_omega;
	float pitch_torque;
	uint32_t sequence;
};

struct ImuInfoData {
	float yaw_total_angle;
	float pitch;
	float yaw_omega;
	uint32_t sequence;
};

int DecodeRemoteControl(const uint8_t *data, uint8_t dlc, RemoteControlData *out);
int DecodeAutoAimInfo(const uint8_t *data, size_t len, AutoAimInfoData *out);
int DecodeImuInfo(const uint8_t *data, size_t len, ImuInfoData *out);
int EncodeGimbalInfo(const GimbalInfoData *in, uint8_t out[16]);

int IngestCanFrame(uint16_t can_id, uint8_t dlc, const uint8_t *data);
int GetLatestRemoteControl(RemoteControlData *out);
int GetLatestAutoAimInfo(AutoAimInfoData *out);
int GetLatestImuInfo(ImuInfoData *out);

}  // namespace rm_test::app::protocols::mcu_link

#endif /* RM_TEST_APP_PROTOCOLS_MCU_LINK_MCU_LINK_H_ */
