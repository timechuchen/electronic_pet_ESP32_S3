# 兰洛电子宠物 Windows 烧录说明

适用开发板：**Waveshare ESP32-S3-Touch-AMOLED-1.75C，32 MB Flash**。随包固件是 2026 年 11 月 18 日的兰洛中文界面版本，包含离线角色动画和触摸互动。

烧录会替换开发板当前程序。此工具只负责 Windows 烧录；已有的 AI 联机服务端仍使用 Mac，未在本次改动中移植到 Windows。

## 方式一 直接烧录现成固件

这种方式不需要安装完整 ESP-IDF，也不需要原 PC 项目的 Node.js、模型目录或 Mac Python 环境。

### 准备环境

1. 在 Windows 安装 Python 3.10 或更新版本，安装时启用 Python 的 PATH 或启动器选项。
2. 打开命令提示符，安装烧录依赖：

```bat
python -m pip install "esptool>=5,<6"
```

如果系统只有 `py` 命令，使用：

```bat
py -3 -m pip install "esptool>=5,<6"
```

依赖需要安装到运行脚本的同一个 Python 环境。若已经在 ESP-IDF v6.1.1 终端中，通常可直接使用其自带工具，不要为此单独升级 SDK 内的 esptool。

Python 版本和安装方法参见 [Espressif esptool 官方安装说明](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html)。首次安装依赖需要网络；准备完毕后，现成固件可离线烧录。

### 开始烧录

1. 将项目里的 `dist/lanluo-windows-flash.zip` 复制到 Windows，**完整解压**，例如放到 `D:\lanluo-windows-flash`。也可以直接使用完整项目目录。
2. 使用 USB 数据线连接开发板，关闭串口监视器和占用该设备的服务端。
3. 双击 **`Windows烧录.bat`**。
4. 脚本先校验固件 SHA-256。只发现一块原生 ESP32 USB 设备时自动选择；有多块时输入对应序号。无法自动识别时会列出串口供手动选择。
5. 等待出现“烧录成功，设备已复位”，随后开发板应显示兰洛中文界面。

脚本窗口会保留结果，按任意键关闭。无需以管理员身份运行，前提是系统已正常识别 COM 串口。

### 指定串口或降低速度

在命令提示符中进入解压目录后执行：

```bat
Windows烧录.bat --port COM5
Windows烧录.bat --port COM5 --baud 115200
```

默认速度为 460800。串口名称以设备管理器中的实际结果为准，示例 COM5 不是固定值。

其他命令：

```bat
Windows烧录.bat --list-ports
Windows烧录.bat --port COM5 --dry-run
Windows烧录.bat --help
```

`--dry-run` 只校验文件并显示命令，不打开串口、不烧录。用于批处理且不想在结束时停留，可先执行 `set PET_NO_PAUSE=1`。

## 方式二 修改源码后编译并烧录

这种方式需要**完整项目源码**及已安装、已激活的 Windows ESP-IDF 环境，建议使用与本项目实机验证一致的 **v6.1.1**。

1. 按 [Espressif Windows 安装说明](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/windows-setup.html) 使用 EIM 安装 SDK 和 ESP32-S3 工具链；选择指定版本可使用自定义安装。
2. 将完整项目放到较短、无空格的英文路径，例如 `D:\esp\electronic_pet`。不要将 Mac 的 ESP-IDF 工具链或 Python 链接当作 Windows 运行环境。
3. 打开安装工具提供的 ESP-IDF 终端并激活环境，确保 `python` 和 `IDF_PATH` 对应这套 SDK。
4. 执行：

```bat
cd /d D:\esp\electronic_pet
Windows编译并烧录.bat
```

如需指定端口：

```bat
Windows编译并烧录.bat --port COM5
```

PowerShell 中使用 `cd D:\esp\electronic_pet` 进入目录，再运行 `& '.\Windows编译并烧录.bat' --port COM5`。

编译使用独立的 `build-windows` 目录与 `sdkconfig.windows`，从项目的 `sdkconfig.defaults` 创建配置，不复用 Mac 的 CMake 缓存。首次编译可能需要联网获取依赖。编译失败会立即停止，不会退回去烧录旧固件。

已生成的中文字体 C 文件和离线动画资源随项目保存；仅编译现有版本不需要额外运行字体或动画导出工具。

## 烧录包内容

```text
Windows烧录.bat
Windows烧录说明.md
tools/windows_flash.py
firmware/manifest.json
firmware/bootloader.bin
firmware/partition-table.bin
firmware/lanluo_pet.bin
```

脚本从文件所在目录寻找固件，不依赖固定盘符，也不使用 Mac 的串口名称。烧录地址分别是 `0x0`、`0x8000`、`0x10000`；目标固定为 ESP32-S3，Flash 设置来自当前已验证的构建。

`Windows烧录.bat` 默认使用 `firmware/` 中打包好的版本。改过源码后需要运行编译脚本；若要更新现成烧录包，在完整项目根目录执行：

```bat
python tools\package_windows.py --build-dir build-windows
```

在 Mac 上打包已完成的构建，则使用：

```bash
./companion/python/bin/python tools/package_windows.py --build-dir build
```

打包工具只收集上面列出的固件和工具文件，不包含 API Key、聊天记录、配置目录或 Mac 依赖链接。

## 常见问题

| 提示或现象 | 处理方法 |
| --- | --- |
| `Python 3.10+ was not found` | 安装合适版本的 Python，或从已激活的 ESP-IDF 终端运行 |
| 缺少 esptool/pyserial | 在脚本实际使用的同一个 Python 环境安装 esptool 5.x |
| 未发现 COM 串口 | 检查 USB 数据线、端口和设备管理器；不要只连接充电线 |
| 串口被占用或拒绝访问 | 关闭串口监视器、其他烧录工具和电子宠物服务端，再重试 |
| 一直停在 Connecting | 按 RESET 重试，或拔插 USB；仍无法进入下载模式时，按住 BOOT、按一下 RESET、松开 BOOT 后重试，并重新确认 COM 端口 |
| 固件校验失败 | 重新复制并完整解压烧录包，不要混用不同版本的三个 bin 文件 |
| 编译提示未激活环境 | 使用 Windows 的 ESP-IDF 终端；仅设置 `IDF_PATH` 并不代表工具链已激活 |
| 精简烧录包无法编译 | ZIP 只包含现成固件；编译需要完整项目源码 |
| 烧录后可触摸但不能 AI 聊天 | 离线固件运行正常；AI 聊天仍需要已有的 Mac 服务端与 Key 配置 |

## 验证范围

随包 bin 来自已经在这块开发板上烧录验证过的中文版固件。脚本的固件校验、串口选择、命令生成和失败处理在 Mac 上进行逻辑测试，预演不会接触正在运行的开发板。

**当前没有 Windows 实机环境，因此 BAT 执行和 Windows USB 烧录仍需在 Windows 上验证。** 本次创建脚本不会重烧当前连接 Mac 的开发板。
