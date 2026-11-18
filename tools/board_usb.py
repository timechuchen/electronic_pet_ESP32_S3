"""Minimal USB diagnostic transport for Lanluo firmware (no PC application code)."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import zlib

MAX_PAYLOAD = 98304


def packet(kind, seq, payload=b''):
    if len(payload) > MAX_PAYLOAD:
        raise ValueError('Payload exceeds firmware limit')
    return struct.pack('<4sIII', b'LPET', kind, seq, len(payload)) + payload + struct.pack('<I', zlib.crc32(payload))


def read_line(link, timeout=4, limit=4096):
    deadline = time.monotonic() + timeout
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = link.read_until(b'\n', size=limit - len(data))
        data.extend(chunk)
        if data.endswith(b'\n'):
            return bytes(data).strip()
        if len(data) >= limit:
            raise ValueError('Diagnostic line exceeds limit')
    raise TimeoutError('开发板应答超时，请确认固件匹配并关闭其他串口程序')


def receive_ack(link, seq, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = read_line(link, max(.01, deadline - time.monotonic()))
        if line.startswith(b'PET_ACK '):
            ack = json.loads(line[8:])
            if ack.get('seq') == seq:
                return ack
        if b'Guru Meditation' in line or b'abort()' in line:
            raise RuntimeError('Firmware restarted unexpectedly')
    raise TimeoutError('未收到匹配的 ACK')


def candidate_ports(ports, platform=None):
    platform = platform or sys.platform
    return sorted(p.device for p in ports if p.vid == 0x303A and p.pid == 0x1001
                  and (platform != 'darwin' or p.device.startswith('/dev/cu.')))


def open_board(port=None):
    import serial
    from serial.tools import list_ports
    if port is None:
        ports = candidate_ports(list_ports.comports())
        if len(ports) != 1:
            raise RuntimeError('请连接一块目标开发板，或用 --port 指定串口；发现：' + ', '.join(ports))
        port = ports[0]
    if sys.platform == 'darwin':
        owners = subprocess.run(['/usr/sbin/lsof', '-t', port], capture_output=True, text=True, timeout=3).stdout.split()
        if any(pid != str(os.getpid()) for pid in owners):
            raise RuntimeError('串口被占用，请先退出 PC 客户端或暂停自动连接，并关闭串口监视器')
    link = serial.Serial()
    link.port = port
    link.baudrate = 921600
    link.timeout = .08
    link.write_timeout = 3
    link.dtr = False
    link.rts = False
    if os.name != 'nt':
        link.exclusive = True
    try:
        link.open()
        deadline = time.monotonic() + 3.5
        while time.monotonic() < deadline:
            link.read_until(b'\n')
        link.write(packet(3, 0, b'status'))
        if not receive_ack(link, 0).get('ok'):
            raise RuntimeError('兰洛固件握手失败')
        return link
    except Exception:
        link.close()
        raise


def save_rgb565_png(destination, width, height, stride, data):
    if width <= 0 or height <= 0 or width > 1024 or height > 1024 or stride < width * 2 or len(data) != stride * height:
        raise ValueError('Invalid framebuffer dimensions or size')
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter: None
        for x in range(width):
            color = struct.unpack_from('<H', data, y * stride + x * 2)[0]
            rows.extend((((color >> 11) & 31) * 255 // 31, ((color >> 5) & 63) * 255 // 63, (color & 31) * 255 // 31))
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
    Path(destination).write_bytes(png)
