# rm_test

基于 Zephyr 的 HPMicro 应用工程，采用“模块线程 + `zbus` 通道”的分布式组织方式。

默认板卡：`hpm6e00evk_v2`

## 3 分钟入手

1. 入口链路
- [src/main.cpp](src/main.cpp)
- [app/src/app_main.cpp](app/src/app_main.cpp)
- [app/bootstrap/src/bootstrap.cpp](app/bootstrap/src/bootstrap.cpp)

2. 当前主运行链路（active）
- [app/modules/remote_input](app/modules/remote_input)
- [app/modules/chassis](app/modules/chassis)
- [app/services/actuator](app/services/actuator)
- [app/services/chassis](app/services/chassis)

3. 调试与回归
- [app/debug/shell/chassis_tuning_shell.cpp](app/debug/shell/chassis_tuning_shell.cpp)
- [tools/smoke_regression.sh](tools/smoke_regression.sh)
- [docs/SMOKE_REGRESSION.md](docs/SMOKE_REGRESSION.md)

应用私有板定义位于：

`boards/hpmicro/hpm6e00evk_v2/`

当前推荐分层：

- `src/`：C++ 入口，仅做最薄的启动转发
- `app/bootstrap/`：C++ 应用启动、模块基类、模块注册、模块拉起
- `app/debug/`：shell、tracing、调试命令接入
- `app/modules/`：C++ 功能模块，各模块持有自己的线程或工作上下文
- `app/channels/`：基于官方 `zbus` 的 channel 与消息载荷定义
- `app/protocols/`：上层链路协议逻辑，如 `pc_link`
- `app/algorithms/`：控制、滤波、估计、数学工具
- `platform/board/`：板级辅助适配
- `platform/drivers/communication/`：CAN/UART/USB 等传输适配
- `platform/drivers/devices/`：应用自管的设备封装
- `platform/storage/`：文件系统与持久化存储接入
- `conf/`：构建变体配置

当前目录导览与 active/staged 状态说明：

- [docs/DIRECTORY_GUIDE.md](docs/DIRECTORY_GUIDE.md)

目录重构规划：

- [docs/DIRECTORY_RESTRUCTURE_PLAN.md](docs/DIRECTORY_RESTRUCTURE_PLAN.md)

模块状态维护约定：

- [docs/MODULE_LIFECYCLE_POLICY.md](docs/MODULE_LIFECYCLE_POLICY.md)

旧 FreeRTOS 工程的映射方式：

- `Dust_EngineerRobot/Algorithm` -> `app/algorithms/`
- `Dust_EngineerRobot/App` -> `app/modules/` + `app/bootstrap/`
- `Dust_EngineerRobot/Communication` -> `platform/drivers/communication/` + `app/protocols/`
- `Dust_EngineerRobot/Device` -> `platform/drivers/devices/`
- `Dust_EngineerRobot/Drivers` -> 尽量被 Zephyr 或板级适配替代

模块间发布订阅优先采用 Zephyr 官方 `zbus`，不再自建一套总线实现。
项目应用层默认按 C++ 组织，底层 Zephyr 和板级封装仍可按 C/C++ 混合使用。

当前默认能力：

- UART shell
- external flash 上的 LittleFS 文件系统
- shell stats
- thread runtime stats
- thread analyzer
- logging + shell log backend
- coredump(logging backend)

可选增强配置：

- `conf/systemview.conf`：启用 SEGGER SystemView

当前文件系统选型建议：

- 介质：外置 XPI flash 的 `storage_partition`
- 文件系统：LittleFS

说明：

- 这是最适合“参数文件 + 中小规模日志文件”的组合
- 现在 `LittleFS` 已经作为默认配置启用
- 我已经把 `littlefs` 和 `segger` 依赖入口补进 [sdk_glue/west.yml](/Users/panpoming/Documents/hpm-zephyr/sdk_glue/west.yml)
- `SystemView` 仍然需要当前板级后续补 RTT 支撑后才能启用

详细架构说明见：

`docs/ARCHITECTURE_REVIEW.md`

调试与存储选型说明见：

`docs/DEBUG_FEATURES.md`

## Build

推荐本地开发使用本工程目录内的构建目录：

- 构建输出：`applications/rm_test/build`
- 这样在 VS Code 里只打开 `applications/rm_test` 也能保持工程自洽。

推荐命令：

```bash
./tools/build_local.sh
```

可选指定 `CONF_FILE`（例如打开 SystemView）：

```bash
./tools/build_local.sh "prj.conf;conf/systemview.conf"
```

你也可以直接执行 west 命令（同样输出到本目录 build）：

```bash
CCACHE_DISABLE=1 ../../.venv/bin/west build -p always -b hpm6e00evk_v2 -s . -d build
```

首次环境准备仍建议在仓库根目录执行：

```bash
../../.venv/bin/west update
```

## Flash

```bash
./.venv/bin/west flash
```
