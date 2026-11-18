"""USB transport for Electron. JSON-lines over stdio; bounded binary packets to ESP32.
No network listener or credentials. Reconnects after unplugging or firmware restart.
"""
import base64
import json
import queue
import struct
import sys
import threading
import time
import zlib
import serial
from serial.tools import list_ports

requests = queue.Queue(maxsize=16)


def emit(data):
    print(json.dumps(data, separators=(',', ':')), flush=True)


def input_reader():
    for line in sys.stdin:
        try:
            data = json.loads(line)
            requests.put(data)
        except (ValueError, TypeError):
            continue
    requests.put(None)


def packet(kind, seq, payload):
    if len(payload) > 98304:
        raise ValueError('Packet too large')
    return struct.pack('<4sIII', b'LPET', kind, seq, len(payload)) + payload + struct.pack('<I', zlib.crc32(payload))


def receive(link, seq, timeout=4):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        raw = link.read_until(b'\n', size=4096)
        if not raw:
            continue
        line = raw.decode('utf-8', errors='replace').strip()
        for prefix, event in [('PET_EVENT ', 'event'), ('PET_STATUS ', 'device'), ('PET_ACK ', 'ack')]:
            if line.startswith(prefix):
                try:
                    value = json.loads(line[len(prefix):])
                except ValueError:
                    break
                if event == 'ack':
                    if value.get('seq') == seq:
                        return value
                else:
                    emit({'type': event, **value})
                break
        else:
            if 'Guru Meditation' in line or 'abort()' in line:
                raise RuntimeError('Firmware restarted unexpectedly')
    raise TimeoutError('Device acknowledgement timed out')


def open_board(port=None):
    ports = [p.device for p in list_ports.comports() if p.vid == 0x303A and 'cu.' in p.device]
    chosen = port or (ports[0] if ports else None)
    if not chosen:
        raise RuntimeError('Connect the board with USB')
    link = serial.Serial()
    link.port = chosen
    link.baudrate = 921600
    link.timeout = .08
    link.write_timeout = 3
    link.dtr = False
    link.rts = False
    link.open()
    # USB control-line changes can reset the board on macOS.
    deadline = time.monotonic() + 3.5
    while time.monotonic() < deadline:
        link.readline()
    link.write(packet(3, 0, b'status'))
    try:
        if not receive(link, 0)['ok']:
            raise RuntimeError('Firmware handshake failed')
    except Exception:
        link.close()
        raise
    emit({'type': 'connection', 'connected': True, 'port': chosen})
    return link


def main():
    threading.Thread(target=input_reader, daemon=True).start()
    link = None
    last_error = ''
    while True:
        req = requests.get()
        if req is None:
            break
        seq = req.get('id', 1)
        try:
            if link is None:
                link = open_board(sys.argv[1] if len(sys.argv) > 1 else None)
                last_error = ''
            payload = base64.b64decode(req.get('data', ''), validate=True)
            kind = int(req['kind'])
            if kind not in (1, 2, 3, 4):
                raise ValueError('Unknown packet kind')
            link.write(packet(kind, seq, payload))
            result = receive(link, seq)
            emit({'type': 'result', 'id': seq, **result})
        except Exception as error:
            if link:
                link.close()
            link = None
            message = str(error)
            if message != last_error:
                emit({'type': 'connection', 'connected': False, 'message': message})
                last_error = message
            emit({'type': 'result', 'id': seq, 'ok': False, 'error': message})
            time.sleep(.4)
    if link:
        link.close()


if __name__ == '__main__':
    main()
