# eMP-gba

GBA 模拟器（Allwinner T113-S3 / TinaLinux，480×480 屏）。

LVGL v9.4.0 UI + vba-next（libretro GBA 内核）+ ALSA 音频输出，架构参考
[eMP-tokenMonitor](https://github.com/ZhangKeLiang0627/eMP-tokenMonitor)
（HAL / Model / View）。

## 特性

- GBA 原生 240×160，等比放大 2 倍到 480×320，顶部居中显示
- 底部 480×160 区域放置虚拟按键（十字键 / A B L R / START SELECT，布局参考 lv_gba_emu）
- ALSA 音频输出（PCM `default`，S16_LE 双声道），音量 0-100，可关闭
- ROM 选择菜单（扫描目录下 `*.gba`），SELECT 长按 2 秒保存存档并返回菜单
- 架构参考 eMP-tokenMonitor：HAL（fbdev / evdev / FS）、Model（GBA 生命周期 + LVGL 线程）、View（UI 封装）

## 目录结构

```
eMP-gba
├── CMakeLists.txt                # 构建主入口（CMake）
├── main.cpp                      # 入口：HAL::Init + Page::Model + 空闲主线程
├── build.sh                      # 交叉编译快捷脚本（T113-S3）
├── cmake/
│   └── build_for_t113s3.cmake    # T113-S3 交叉编译工具链文件
├── inc/                          # HAL.h / Model.h / View.h / common_inc.h
├── src/
│   ├── HAL/HAL.cpp               # fbdev + evdev + POSIX FS 初始化
│   ├── Page/Model.cpp            # GBA 生命周期 + LVGL 线程
│   ├── Page/View.cpp             # 菜单 / 模拟器 UI 的 C++ 封装
│   ├── gba_core/                 # lv_gba_emu 移植层（视图 / 菜单 / libretro 桥）
│   └── gba_port/                 # T113 端口（ALSA 音频、POSIX FS、port 桩）
└── libs/
    ├── lvgl                      # submodule：LVGL v9.4.0
    ├── vba-next                  # submodule：FASTSHIFT/vba-next（libretro GBA core）
    └── lv_conf.h                 # LVGL 配置（480×480 RGB565 / fbdev / evdev）
```

## 依赖

- 交叉编译工具链 [eMP-toolchain](https://github.com/ZhangKeLiang0627/eMP-toolchain)
  （GCC 6.4.1 musl，ARMv7-A / Cortex-A7）
- submodule：
  - `libs/lvgl` → [lvgl/lvgl](https://github.com/lvgl/lvgl) @ v9.4.0
  - `libs/vba-next` → [FASTSHIFT/vba-next](https://github.com/FASTSHIFT/vba-next) @ `a5e0d8ad`（与 lv_gba_emu 一致的钉定版本，含 `gba_set_rom_size`）

## 构建

CMake 为主（推荐），`T113_SDK` 默认指向 `/home/hugokkl/eMP-t113-toolchain`。

```sh
git clone --recursive https://github.com/ZhangKeLiang0627/eMP-gba.git
cd eMP-gba

# 方式一：build.sh
T113_SDK=/path/to/eMP-toolchain ./build.sh

# 方式二：直接 CMake
cmake -B build \
      -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_t113s3.cmake \
      -DT113_SDK=/path/to/eMP-toolchain
make -C build -j$(nproc)
```

产物：`build/eMP_gba`（ARM 可执行文件）。

## 运行（T113-S3 板端）

```sh
# 环境变量（可选）
export EMP_GBA_ROM_DIR=/root/roms    # ROM 目录，默认 /root
export EMP_GBA_VOLUME=100            # 音量 0-100，0 关闭音频
export LV_GBA_AUDIO_DEVICE=default   # ALSA PCM 设备（可选）

./eMP_gba
```

- 启动进入 ROM 选择菜单，点击 `*.gba` 开始游戏
- 虚拟按键：十字键 / A / B / L / R / START / SELECT
- SELECT 长按 2 秒：保存存档并返回菜单
- 存档：同目录 `.sav` 文件自动读写（ROM 删除时 `.sav` 一并删除）

## 屏幕布局（480×480）

```
┌──────────────────────────────┐
│                              │
│       GBA 画面 480×320       │  ← 240×160 等比放大 2 倍，顶部居中
│                              │
├──────────────────────────────┤
│   十字键   A B L R  START    │  ← 480×160 操作区（参考 lv_gba_emu）
│            SELECT            │
└──────────────────────────────┘
```

## 架构

- HAL：`lv_init` / `lv_fs_posix_init` / fbdev（`/dev/fb0`）/ evdev（`/dev/input/event1`）/ 信号处理
- Model（`Page::Model`）：ROM 目录与音量配置、菜单→模拟→返回菜单生命周期、LVGL 线程（`lv_timer_handler`）
- View（`Page::View`）：`gba_menu` / `lv_gba_emu` 的 C++ 封装（菜单选择桥、退出桥、ALSA 启动）
- gba_core：lv_gba_emu 移植层（视图、菜单、libretro 回调桥接、VFS 实现）
- gba_port：T113 端口——ALSA 音频（`gba_port_audio.c`）、POSIX FS 驱动（`lv_fs_posix.c`）、port 桩（`port_impl.c`）

## 说明

- 该 LVGL 版本未内置 POSIX FS 驱动，项目自带 `lv_fs_posix.c`（盘符 `/`）供 ROM 加载 / 存档 / 菜单使用
- vba-next 为 C++11 老代码，构建统一使用 C++11（见 CMakeLists）
- 按键布局、libretro 桥接等移植层来自 [lv_gba_emu](https://github.com/FASTSHIFT/lv_gba_emu)（MIT）
