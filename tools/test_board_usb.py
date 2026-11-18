import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock
import zlib
import board_usb


class BoardUSBTests(unittest.TestCase):
    def test_packet_matches_firmware_layout_and_crc(self):
        payload=b'\x00\xffJPEG';packet=board_usb.packet(1,42,payload)
        self.assertEqual(struct.unpack('<4sIII',packet[:16]),(b'LPET',1,42,len(payload)))
        self.assertEqual(packet[16:-4],payload)
        self.assertEqual(struct.unpack('<I',packet[-4:])[0],zlib.crc32(payload))
        with self.assertRaises(ValueError):board_usb.packet(1,1,b'0'*98305)

    def test_partial_lines_events_and_unrelated_ack(self):
        link=Mock();link.read_until.side_effect=[b'PET_EVENT {"action":"wave"}\n',b'PET_ACK {"seq":4,"ok":true}\n',b'PET_ACK {"se',b'q":5,"ok":true}\n']
        self.assertTrue(board_usb.receive_ack(link,5)['ok'])

    def test_invalid_and_missing_lines_are_bounded(self):
        link=Mock();link.read_until.return_value=b'1234'
        with self.assertRaises(ValueError):board_usb.read_line(link,limit=4)
        link.read_until.return_value=b''
        with self.assertRaises(TimeoutError):board_usb.read_line(link,timeout=.002)

    def test_firmware_panic_is_reported(self):
        link=Mock();link.read_until.return_value=b'Guru Meditation Error\n'
        with self.assertRaises(RuntimeError):board_usb.receive_ack(link,1)

    def test_usb_discovery_on_mac_and_windows(self):
        port=lambda name,vid=0x303A,pid=0x1001:SimpleNamespace(device=name,vid=vid,pid=pid)
        self.assertEqual(board_usb.candidate_ports([port('/dev/tty.usbmodem1'),port('/dev/cu.usbmodem1')],'darwin'),['/dev/cu.usbmodem1'])
        self.assertEqual(board_usb.candidate_ports([port('COM8'),port('COM3',0x1234)],'win32'),['COM8'])

    def test_framebuffer_png_pixels_padding_and_crc(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'frame.png'
            board_usb.save_rgb565_png(path,2,1,6,struct.pack('<HHH',0xf800,0x07e0,0))
            data=path.read_bytes();self.assertEqual(data[:8],b'\x89PNG\r\n\x1a\n')
            at=8;parts={}
            while at<len(data):
                size=struct.unpack_from('>I',data,at)[0];kind=data[at+4:at+8];body=data[at+8:at+8+size]
                crc=struct.unpack_from('>I',data,at+8+size)[0];self.assertEqual(crc,zlib.crc32(kind+body));parts[kind]=body;at+=12+size
            self.assertEqual(zlib.decompress(parts[b'IDAT']),b'\0\xff\0\0\0\xff\0')
            with self.assertRaises(ValueError):board_usb.save_rgb565_png(path,2,1,4,b'\0')


if __name__=='__main__':unittest.main()
