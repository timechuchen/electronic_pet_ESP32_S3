# 兰洛电子宠物 Windows 源码编译烧录说明

适用开发板：**Waveshare ESP32-S3-Touch-AMOLED-1.75C，32 MB Flash**。

**暂不提供预编译固件或烧录包。** 请下载或克隆完整源码，安装 ESP-IDF 后编译烧录。PC 独立桌宠安装包不能用于烧录开发板。

烧录会替换开发板当前程序。开始前关闭串口监视器和占用该设备的服务端；联机客户端的源码和安装包统一见 [PC 项目](https://github.com/timechuchen/electronic_pet_PC)。

## 准备与烧录

需要**完整项目源码**及已安装、已激活的 Windows ESP-IDF 环境，建议使用与本项目实机验证一致的 **v6.1.1**。

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

`Windows烧录.bat` 也会先编译源码，再烧录。不要在未激活 ESP-IDF 的普通终端中直接运行。

## 常见问题

| 提示或现象 | 处理方法 |
| --- | --- |
| Python 或 ESP-IDF 未找到 | 打开并激活安装工具提供的 ESP-IDF 终端 |
| 未发现 COM 串口 | 检查 USB 数据线、端口和设备管理器；不要只连接充电线 |
| 串口被占用 | 关闭串口监视器、其他烧录工具和电子宠物服务端 |
| 一直停在 Connecting | 按 RESET 或拔插 USB；仍不行时按住 BOOT、按一下 RESET、松开 BOOT，再确认 COM 端口 |
| 编译失败 | 根据编译日志修复后重试；脚本不会退回烧录旧固件 |
| 烧录后可触摸但不能 AI 聊天 | AI 聊天仍需要独立 PC 客户端和 Key 配置 |

## 验证范围

中文版固件已在开发板上验证。Windows 烧录脚本通过跨平台逻辑测试，**BAT 执行和 Windows USB 烧录尚未在 Windows 实机验证**。
