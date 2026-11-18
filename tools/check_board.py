"""Exercise the flashed board and save its actual LVGL framebuffer (no simulator)."""
import json
import pathlib
import struct
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'companion'))
from serial_worker import open_board, packet

link = open_board()
seq = 1000
out = ROOT / 'artifacts'
out.mkdir(exist_ok=True)


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
        line = link.readline().strip()
        if line.startswith(b'PET_SNAPSHOT '):
            w, h, stride, size = map(int, line.split()[1:])
            data = bytearray()
            while len(data) < size and time.monotonic() < until:
                data.extend(link.read(size - len(data)))
            assert len(data) == size, (len(data), size)
            rgb = bytearray()
            for y in range(h):
                for x in range(w):
                    color = struct.unpack_from('<H', data, y * stride + x * 2)[0]
                    rgb.extend((((color >> 11) & 31) * 255 // 31,
                                ((color >> 5) & 63) * 255 // 63,
                                (color & 31) * 255 // 31))
            ppm = out / f'{snapshot_name}.ppm'
            ppm.write_bytes(f'P6\n{w} {h}\n255\n'.encode() + rgb)
            subprocess.run(['sips', '-s', 'format', 'png', str(ppm), '--out', str(out / f'{snapshot_name}.png')], check=True, capture_output=True)
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
    if '--chinese' in sys.argv:
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
    send(3, b'reset')
    link.close()
