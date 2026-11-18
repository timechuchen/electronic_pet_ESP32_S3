"""Windows flashing entry point. --dry-run never opens a serial port or writes flash."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = {0x0: "bootloader.bin", 0x8000: "partition-table.bin", 0x10000: "lanluo_pet.bin"}
FLASH_ARGS = ["--flash-mode", "dio", "--flash-size", "32MB", "--flash-freq", "80m"]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def manifest_from_build(build):
    build = Path(build).resolve()
    info = json.loads((build / "project_description.json").read_text(encoding="utf-8"))
    args = json.loads((build / "flasher_args.json").read_text(encoding="utf-8"))
    if info.get("target") != "esp32s3" or info.get("project_name") != "lanluo_pet":
        raise ValueError("构建目录不是本项目的 ESP32-S3 固件。")
    if args.get("write_flash_args") != FLASH_ARGS:
        raise ValueError("构建的 Flash 参数发生变化，请先检查打包脚本。")
    files, sources = [], []
    for address, relative in args["flash_files"].items():
        offset = int(address, 0)
        if offset not in LAYOUT:
            raise ValueError("发现未预期的烧录地址。")
        source = (build / relative).resolve()
        if not source.is_relative_to(build) or not source.is_file():
            raise ValueError("构建文件路径无效。")
        files.append({"offset": hex(offset), "file": LAYOUT[offset], "sha256": sha256(source)})
        sources.append((source, LAYOUT[offset]))
    if {int(f["offset"], 0) for f in files} != set(LAYOUT) or len(files) != 3:
        raise ValueError("固件必须包含引导程序、分区表和应用三个文件。")
    return {"schema": 1, "project": "lanluo_pet", "chip": "esp32s3",
            "idf_version": info.get("git_revision"), "files": files}, sources


def validate_firmware(folder):
    folder = Path(folder)
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    if (manifest.get("schema"), manifest.get("project"), manifest.get("chip")) != (1, "lanluo_pet", "esp32s3"):
        raise ValueError("固件清单与当前开发板项目不匹配。")
    files = manifest.get("files", [])
    if len(files) != 3 or {int(f["offset"], 0) for f in files} != set(LAYOUT):
        raise ValueError("固件文件或烧录地址不完整。")
    plan = []
    end = 0
    for entry in sorted(files, key=lambda f: int(f["offset"], 0)):
        offset = int(entry["offset"], 0)
        if entry["file"] != LAYOUT[offset]:
            raise ValueError("固件文件名与烧录布局不匹配。")
        path = folder / entry["file"]
        if not path.is_file() or not path.stat().st_size:
            raise ValueError(f"固件文件缺失或为空：{path}")
        if sha256(path) != entry["sha256"]:
            raise ValueError(f"固件校验失败，请重新复制烧录包：{path.name}")
        if offset < end:
            raise ValueError("固件地址发生重叠。")
        end = offset + path.stat().st_size
        if end > 32 * 1024 * 1024:
            raise ValueError("固件超过 32 MB Flash 容量。")
        plan.extend([hex(offset), str(path.resolve())])
    return plan


def port_name(value):
    if not re.fullmatch(r"COM[1-9][0-9]*", value, flags=re.IGNORECASE):
        raise ValueError("串口格式应为 COM3、COM7 等。")
    return value.upper()


def choose_port(ports, read=input):
    if not ports:
        raise ValueError("未发现 COM 串口。请连接 USB 数据线，并检查设备管理器。")
    preferred = [p for p in ports if p.vid == 0x303A and p.pid == 0x1001]
    if len(preferred) == 1:
        return port_name(preferred[0].device)
    candidates = preferred or ports
    print("发现多个目标设备，请选择：" if preferred else "未识别到原生 ESP32 USB，请手动选择串口：")
    for index, port in enumerate(candidates, 1):
        print(f"  {index}. {port.device}  {port.description}")
    answer = read("请输入序号，或输入 q 退出：").strip()
    if answer.lower() == "q":
        raise KeyboardInterrupt
    if not answer.isdigit() or not 1 <= int(answer) <= len(candidates):
        raise ValueError("串口选择无效，请重新运行。")
    return port_name(candidates[int(answer) - 1].device)


def flash_command(port, baud, plan):
    return [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port_name(port),
            "--baud", str(baud), "--before", "default-reset", "--after", "hard-reset",
            "write-flash", *FLASH_ARGS, *plan]


def build_command(root=ROOT):
    sdk = os.environ.get("IDF_PATH", "")
    if not sdk or not (Path(sdk) / "tools/idf.py").is_file():
        raise ValueError("请先打开 Windows ESP-IDF 终端并激活环境，再运行编译脚本。建议使用 v6.1.1。")
    if not (root / "main/main.c").is_file():
        raise ValueError("当前为精简烧录包，没有项目源码。请使用完整项目进行编译。")
    return [sys.executable, str(Path(sdk) / "tools/idf.py"), "-B", str(root / "build-windows"),
            "-D", f"SDKCONFIG={root / 'sdkconfig.windows'}", "build"]


def main(argv=None):
    parser = argparse.ArgumentParser(description="兰洛 ESP32-S3 Windows 烧录工具")
    parser.add_argument("--port", help="指定串口，例如 COM5；不填写则自动识别或选择")
    parser.add_argument("--baud", type=int, choices=[115200, 460800, 921600], default=460800)
    parser.add_argument("--build", action="store_true", help="先在已激活的 ESP-IDF 环境中编译源码")
    parser.add_argument("--dry-run", action="store_true", help="只检查文件和显示命令，不编译、不访问设备")
    parser.add_argument("--list-ports", action="store_true", help="列出当前串口后退出")
    args = parser.parse_args(argv)
    if args.port:
        args.port = port_name(args.port)
    if not args.dry_run and os.name != "nt":
        raise ValueError("此入口用于 Windows；Mac 请使用现有的 .command 脚本。")
    if args.list_ports:
        from serial.tools import list_ports
        for port in list_ports.comports():
            print(f"{port.device}: {port.description}")
        return 0
    if not args.dry_run:
        if not importlib.util.find_spec("esptool") or not importlib.util.find_spec("serial"):
            raise ValueError('缺少 esptool/pyserial。请在同一个 Python 环境运行：python -m pip install "esptool>=5,<6"')
        from importlib.metadata import version
        if version("esptool").split(".")[0] != "5":
            raise ValueError("该脚本使用 esptool 5.x 命令格式，请使用对应版本或 ESP-IDF v6.1.1 环境。")
    if args.build:
        command = build_command()
        print("编译命令：", subprocess.list2cmdline(command), flush=True)
        if args.dry_run:
            print("预演结束：实际运行时会编译至 build-windows，再烧录本次生成的固件。")
            return 0
        subprocess.run(command, cwd=ROOT, check=True)
        _, sources = manifest_from_build(ROOT / "build-windows")
        plan = []
        for source, name in sorted(sources, key=lambda pair: next(k for k, v in LAYOUT.items() if v == pair[1])):
            offset = next(k for k, v in LAYOUT.items() if v == name)
            plan.extend([hex(offset), str(source)])
    else:
        plan = validate_firmware(ROOT / "firmware")
        print("中文版固件文件校验通过。", flush=True)
    port = args.port
    if not port and args.dry_run:
        raise ValueError("预演请指定 --port COM3，以避免访问设备。")
    if not port:
        from serial.tools import list_ports
        port = choose_port(list(list_ports.comports()))
    command = flash_command(port, args.baud, plan)
    print(f"目标：ESP32-S3 / 32 MB Flash / {port}。烧录会替换设备当前固件。")
    print("请先关闭串口监视器及占用设备的电子宠物服务端。")
    print("烧录命令：", subprocess.list2cmdline(command), flush=True)
    if args.dry_run:
        print("预演通过：未连接设备，未烧录。")
        return 0
    subprocess.run(command, cwd=ROOT, check=True)
    print("烧录成功，设备已复位。等待出现兰洛中文界面即可。")
    print("本脚本只烧录固件，不提供 Windows 版 AI 联机服务端。")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\n已取消。")
        raise SystemExit(130)
    except subprocess.CalledProcessError as error:
        print(f"操作失败，退出码 {error.returncode}。请查看上方工具日志；检查端口占用、数据线或按 RESET 后重试。", file=sys.stderr)
        raise SystemExit(error.returncode or 1)
    except (OSError, ValueError, ImportError, KeyError, TypeError) as error:
        print(f"错误：{error}", file=sys.stderr)
        raise SystemExit(1)
