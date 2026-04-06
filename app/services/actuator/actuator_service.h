/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RM_TEST_APP_SERVICES_ACTUATOR_ACTUATOR_SERVICE_H_
#define RM_TEST_APP_SERVICES_ACTUATOR_ACTUATOR_SERVICE_H_

#include <stdint.h>

namespace rm_test::app::services::actuator {

enum class MotorCurrentGroup : uint8_t {
	kDji0x200 = 0,
	kDji0x1ff,
};

int SendMotorCurrent(MotorCurrentGroup group, const int16_t current_cmd[4]);
int SendDjiCurrentGroup200(const int16_t current_cmd[4]);
int SendDjiCurrentGroup1ff(const int16_t current_cmd[4]);

}  // namespace rm_test::app::services::actuator

#endif /* RM_TEST_APP_SERVICES_ACTUATOR_ACTUATOR_SERVICE_H_ */
