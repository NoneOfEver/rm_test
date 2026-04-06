/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RM_TEST_PLATFORM_DRIVERS_COMMUNICATION_CAN_DISPATCH_H_
#define RM_TEST_PLATFORM_DRIVERS_COMMUNICATION_CAN_DISPATCH_H_

#include <stdint.h>

namespace rm_test::platform::drivers::communication::can_dispatch {

int Initialize();
int SendStdData(uint16_t can_id, const uint8_t data[8], uint8_t dlc = 8U);

}  // namespace rm_test::platform::drivers::communication::can_dispatch

#endif /* RM_TEST_PLATFORM_DRIVERS_COMMUNICATION_CAN_DISPATCH_H_ */
