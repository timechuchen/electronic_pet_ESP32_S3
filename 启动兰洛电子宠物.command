#!/bin/zsh
set -e
PROJECT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
cd "$PROJECT_DIR"
PYTHON="$PROJECT_DIR/companion/python/bin/python3"
if [[ ! -x "$PYTHON" ]]; then
  echo '找不到 Python 环境，请查看 README.md 的运行环境说明。'
  exit 1
fi
# Resolve the application bundle path: macOS can reject a symlinked .app path.
ELECTRON="$("$PYTHON" -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve())' "$PROJECT_DIR/companion/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron")"
if [[ ! -x "$ELECTRON" ]]; then
  echo '找不到 Electron，请查看 README.md 的运行环境说明。'
  exit 1
fi
unset ELECTRON_RUN_AS_NODE
mkdir -p artifacts
echo '正在启动兰洛。连接开发板后会自动同步画面；Cmd+J 打开聊天，Cmd+, 打开设置。'
exec "$ELECTRON" "$PROJECT_DIR/companion/shell" "$@" > >(tee -a artifacts/companion.log) 2>&1
