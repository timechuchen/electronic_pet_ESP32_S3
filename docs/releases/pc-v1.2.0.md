# PC 桌宠 v1.2.0 下载

本次提供 Mac 和 Windows 两个已有的 PC 独立桌宠安装包。ESP32 预编译烧录包已撤下，暂时仅支持从本仓库源码编译烧录，步骤见 [README](https://github.com/timechuchen/electronic_pet_ESP32_S3#快速开始)。**PC 包不包含 ESP32 USB 联机桥接，不能替代本仓库的 Mac 服务端。** ESP32 联机体验仍使用 README 中的源码启动方式。

## 下载与使用

| 附件 | 内容 | 使用方式 |
| --- | --- | --- |
| `Lanluo-Desktop-1.2.0-macOS-AppleSilicon.zip` | 原版 Mac PC 桌宠 v1.2.0 | 适用 Apple Silicon、macOS 13+；解压后打开 `兰洛桌宠.app`，或将整个应用拖到“应用程序” |
| `Lanluo-Desktop-1.2.0-Windows-x64.zip` | 原版 Windows PC 桌宠 v1.2.0 | 适用 Windows 10/11 x64；完整解压后打开 `Lanluo-1.2.0-Windows/Lanluo.exe`，不能只复制 exe |
| `SHA256SUMS.txt` | 本次附件的 SHA-256 | 下载后核对文件完整性 |

PC 两个压缩包来自 `CustomElectronicPet/releases/v1.2.0`，没有重新构建或改动压缩包内容，仅使用英文附件名便于下载；SHA-256 已与原始发布校验文件核对一致。

附件中的 `PC-Usage-1.2.0.zh-CN.md`、`PC-AI-Setup-1.2.0.zh-CN.md`、`PC-VRM-FBX-1.2.0.zh-CN.md` 和 `PC-Build-Verification-1.2.0.json` 保留原发布目录的使用、AI、模型和验证说明。说明中的原中文压缩包名与上述英文附件一一对应。

## 已知限制

- Mac PC 包是 ad-hoc 签名，未经过 Apple Developer ID 签名与公证；Windows PC 包未做发布者签名。首次打开按系统的应用允许流程处理，不需要关闭全局安全保护。
- 原 Windows PC 包完成了交叉打包和资源检查，尚未完成 Windows 实机启动、系统加密、摄像头、音频和语音验收。
- 原 Mac PC 包的真实系统密钥存储测试曾超过 30 秒，Key 保存步骤未验收通过；聊天界面和模拟回复测试不能替代系统钥匙串验证。具体情况以随附原版说明为准。
- AI 对话需要自行配置 DeepSeek Key，发布文件不含个人 Key 或聊天记录。
- ESP32 固件已在开发板验证；Windows 烧录脚本通过逻辑测试，Windows USB 烧录尚未实测。

仓库保持私有，下载附件需要有仓库访问权限。该 Release 标签仅用于分发 PC v1.2.0 原包，不表示把 ESP32 工程版本升级到了 1.2.0。
