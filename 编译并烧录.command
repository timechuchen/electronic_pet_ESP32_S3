#!/bin/zsh
set -e
PROJECT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
cd "$PROJECT_DIR"
echo '请先从菜单栏退出兰洛电子宠物，并关闭串口监视器。'
if [[ -z "${IDF_PATH:-}" || ! -f "$IDF_PATH/tools/idf.py" ]]; then
  PET_ACTIVATE="${PET_IDF_ACTIVATE:-$HOME/.espressif/tools/activate_idf_v6.1.1.sh}"
  if [[ ! -f "$PET_ACTIVATE" ]]; then
    echo '请先打开已激活的 ESP-IDF 终端，或将 PET_IDF_ACTIVATE 设置为本机激活脚本的路径。'
    exit 1
  fi
  source "$PET_ACTIVATE"
fi
PET_PORT="$(python -c 'from serial.tools import list_ports; print(next((p.device for p in list_ports.comports() if p.vid == 0x303A and "cu." in p.device), ""))')"
if [[ -z "$PET_PORT" ]]; then
  echo '未发现开发板，请连接 USB 数据线后重试。'
  exit 1
fi
if /usr/sbin/lsof -t "$PET_PORT" >/dev/null 2>&1; then
  echo '串口正被占用。请退出兰洛服务端或串口监视器后再次运行。'
  exit 1
fi
python "$IDF_PATH/tools/idf.py" -p "$PET_PORT" build flash
echo '烧录完成。可运行「启动兰洛电子宠物.command」进入联机模式。'
