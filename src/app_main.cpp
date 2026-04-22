/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <src/app_main.h>
#include <bootstrap/bootstrap.h>

namespace rm_test::app {

int Main()
{
	bootstrap::Bootstrap bootstrap;

	return bootstrap.Run();
}

}  // namespace rm_test::app
