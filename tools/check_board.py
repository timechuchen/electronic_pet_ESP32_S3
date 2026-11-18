"""Exercise the flashed board and save its actual LVGL framebuffer (no simulator)."""
import json
import pathlib
import argparse
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
from board_usb import open_board, packet, read_line, save_rgb565_png

link = None
seq = 1000
out = ROOT / 'artifacts'


def send(kind, body=b'', corrupt=False, snapshot_name='offline-screen'):
    global seq
    seq += 1
    raw = packet(kind, seq, body)
    if corrupt:
        raw = raw[:-1] + bytes([raw[-1] ^ 1])
    link.write(raw)
    result = {}
    until = time.monotonic() + 10
    while time.monotonic() < until:
        line = read_line(link, max(.01, until - time.monotonic()))
        if line.startswith(b'PET_SNAPSHOT '):
            w, h, stride, size = map(int, line.split()[1:])
            if not (0 < w <= 1024 and 0 < h <= 1024 and w * 2 <= stride <= 4096 and size == stride * h):
                raise ValueError('Invalid framebuffer header')
            data = bytearray()
            while len(data) < size and time.monotonic() < until:
                data.extend(link.read(size - len(data)))
            assert len(data) == size, (len(data), size)
            save_rgb565_png(out / f'{snapshot_name}.png', w, h, stride, data)
            result['snapshot'] = [w, h]
        elif line.startswith(b'PET_STATUS '):
            result['status'] = json.loads(line[11:])
        elif line.startswith(b'PET_ACK '):
            ack = json.loads(line[8:])
            if ack['seq'] == seq:
                result.update(ack)
                return result
        elif b'Guru Meditation' in line or b'abort()' in line:
            raise RuntimeError(line)
    raise TimeoutError(result)


def main():
    global link, out
    parser = argparse.ArgumentParser(description='兰洛固件实机检查与屏幕截图；先退出 PC 客户端释放串口')
    parser.add_argument('--port', help='串口，如 COM5 或 /dev/cu.usbmodem2101；默认自动发现')
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'artifacts')
    parser.add_argument('--chinese', action='store_true', help='额外检查五页中文按钮和聊天提示')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    link = open_board(args.port)
    try:
        report = {}
        report['initial'] = send(3, b'status')
        assert report['initial']['status']['decoded'] > 0
        report['bad_crc'] = send(3, b'status', corrupt=True)
        assert not report['bad_crc']['ok']
        report['bad_jpeg'] = send(1, b'not a jpeg')
        assert not report['bad_jpeg']['ok']
        report['jpeg'] = send(1, (ROOT / 'assets/idle.jpg').read_bytes())
        assert report['jpeg']['ok']
        report['live'] = send(3, b'status')
        assert report['live']['status']['live']
        time.sleep(3)
        report['fallback'] = send(3, b'status')
        assert not report['fallback']['status']['live']
        assert send(3, b'wave')['ok']
        report['wave'] = send(3, b'status')
        assert report['wave']['status']['action'] == 'wave'
        assert send(3, b'reset')['ok']
        time.sleep(.3)
        assert send(3, b'pause')['ok']
        report['paused'] = send(3, b'status')
        assert report['paused']['status']['paused']
        report['snapshot'] = send(3, b'snapshot')
        assert report['snapshot'].get('snapshot') == [466, 466]
        if args.chinese:
            for page in range(2, 6):
                assert send(3, b'more')['ok']
                send(3, b'snapshot', snapshot_name=f'chinese-page-{page}')
            assert send(3, b'pause')['ok']
            assert send(3, b'chat')['ok']
            send(3, b'snapshot', snapshot_name='chinese-offline-chat')
            assert send(3, b'home')['ok']
            assert send(3, b'reset')['ok']
            assert send(1, (ROOT / 'assets/idle.jpg').read_bytes())['ok']
            time.sleep(.2)
            assert send(3, b'chat')['ok']
            send(3, b'snapshot', snapshot_name='chinese-live')
        (out / 'firmware-check.json').write_text(json.dumps(report, indent=2))
        print('FIRMWARE_CHECK_PASS', json.dumps(report))
    finally:
        try:
            send(3, b'reset')
        finally:
            link.close()


if __name__ == '__main__':
    main()
