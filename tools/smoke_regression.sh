#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WS_DIR="$(cd "$ROOT_DIR/../.." && pwd)"
BUILD_DIR="$WS_DIR/build"
BOARD="hpm6e00evk_v2"
TMP_BUILD_DIR="/tmp/rm_test_smoke_can_off"
OVERLAY_FILE="/tmp/rm_test_smoke_can_off.conf"

PASS_COUNT=0

pass() {
  echo "[PASS] $1"
  PASS_COUNT=$((PASS_COUNT + 1))
}

fail() {
  echo "[FAIL] $1" >&2
  exit 1
}

check_file_contains() {
  local file="$1"
  local pattern="$2"
  local title="$3"
  if rg -q "$pattern" "$file"; then
    pass "$title"
  else
    fail "$title (pattern: $pattern, file: $file)"
  fi
}

echo "== rm_test smoke regression =="
echo "ROOT_DIR=$ROOT_DIR"
echo "WS_DIR=$WS_DIR"

echo "-- [1/5] Static contract checks"
check_file_contains "$ROOT_DIR/app/bootstrap/src/bootstrap.cpp" "InitializeInfrastructure\\(" "Bootstrap calls runtime infrastructure init"
check_file_contains "$ROOT_DIR/app/bootstrap/src/bootstrap.cpp" "module_manager_\\.Initialize\\(" "Bootstrap initializes module manager"
check_file_contains "$ROOT_DIR/app/bootstrap/src/bootstrap.cpp" "module_manager_\\.Start\\(" "Bootstrap starts module manager"
check_file_contains "$ROOT_DIR/app/modules/modules_registry.cpp" "RegisterApplicationModules\\(" "Application module hook is implemented"
check_file_contains "$ROOT_DIR/app/modules/modules_registry.cpp" "CONFIG_RM_TEST_MODULE_REMOTE_INPUT" "Remote input module is config-gated"
check_file_contains "$ROOT_DIR/app/modules/modules_registry.cpp" "CONFIG_RM_TEST_MODULE_CHASSIS" "Chassis module is config-gated"
check_file_contains "$ROOT_DIR/app/modules/modules_registry.cpp" "CONFIG_RM_TEST_MODULE_ARM" "Arm module is config-gated"
check_file_contains "$ROOT_DIR/app/modules/modules_registry.cpp" "CONFIG_RM_TEST_MODULE_GIMBAL" "Gimbal module is config-gated"
check_file_contains "$ROOT_DIR/app/modules/modules_registry.cpp" "CONFIG_RM_TEST_MODULE_GANTRY" "Gantry module is config-gated"
check_file_contains "$ROOT_DIR/app/modules/modules_registry.cpp" "CONFIG_RM_TEST_MODULE_REFEREE" "Referee module is config-gated"
check_file_contains "$ROOT_DIR/app/debug/shell/chassis_tuning_shell.cpp" "SHELL_CMD\\(status, NULL, \"Show chassis tuning provider status\"" "Shell exposes chassis pid status command"
check_file_contains "$ROOT_DIR/app/services/chassis/chassis_tuning_service.h" "bool HasProvider\\(\\)" "Tuning service provides provider state query"

echo "-- [2/5] Build default configuration"
cmake --build "$BUILD_DIR" -j8 >/dev/null
pass "Default build succeeds"

ELF_FILE="$BUILD_DIR/zephyr/zephyr.elf"
if [[ -f "$ELF_FILE" ]]; then
  pass "Default ELF exists"
else
  fail "Default ELF missing: $ELF_FILE"
fi

echo "-- [3/5] Build CAN-off configuration"
cat > "$OVERLAY_FILE" <<EOF
CONFIG_RM_TEST_RUNTIME_INIT_CAN=n
EOF

PYTHON_BIN="${WS_DIR}/.venv/bin/python"
if [[ -x "$PYTHON_BIN" ]]; then
  cmake -S "$ROOT_DIR" -B "$TMP_BUILD_DIR" -GNinja -DBOARD="$BOARD" -DPython3_EXECUTABLE="$PYTHON_BIN" -DOVERLAY_CONFIG="$OVERLAY_FILE" >/dev/null
else
  cmake -S "$ROOT_DIR" -B "$TMP_BUILD_DIR" -GNinja -DBOARD="$BOARD" -DOVERLAY_CONFIG="$OVERLAY_FILE" >/dev/null
fi

cmake --build "$TMP_BUILD_DIR" -j8 >/dev/null
pass "CAN-off build succeeds"

CAN_OFF_ELF="$TMP_BUILD_DIR/zephyr/zephyr.elf"
if [[ -f "$CAN_OFF_ELF" ]]; then
  pass "CAN-off ELF exists"
else
  fail "CAN-off ELF missing: $CAN_OFF_ELF"
fi

echo "-- [4/5] Config gate checks"
if rg -q "^# CONFIG_RM_TEST_RUNTIME_INIT_CAN is not set$|^CONFIG_RM_TEST_RUNTIME_INIT_CAN=n$" "$TMP_BUILD_DIR/zephyr/.config"; then
  pass "CAN runtime init is disabled in CAN-off config"
else
  fail "CAN runtime init disable flag missing in $TMP_BUILD_DIR/zephyr/.config"
fi

echo "-- [5/5] Summary"
echo "Smoke regression passed with ${PASS_COUNT} checks."
