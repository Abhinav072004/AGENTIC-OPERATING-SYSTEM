#!/usr/bin/env python3
"""Independent Ethernet peer for the guest's native socket acceptance program."""
import hashlib
import json
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
from boot_test import QMP, validate_diagnostics, validate_screen
from qemu import REPO, command
GUEST_IP = bytes((10, 0, 2, 15))
PEER_IP = bytes((10, 0, 2, 2))
GUEST_MAC = bytes.fromhex('525400123456')
PEER_MAC = bytes.fromhex('020000000001')


def checksum(data):
    if len(data) % 2: data += b'\0'
    value = sum(struct.unpack('!' + 'H' * (len(data) // 2), data))
    while value >> 16: value = (value & 65535) + (value >> 16)
    return (~value) & 65535


def pseudo(source, destination, protocol, data):
    return source + destination + struct.pack('!BBH', 0, protocol, len(data)) + data


class Peer:
    def __init__(self, endpoint):
        self.endpoint = endpoint
        self.address = None
        self.events = []
        self.syns = self.data_attempts = self.timeout_syns = 0
        self.active_port = None
        self.active_next = 5014
        self.passive_next = 9000

    def ethernet(self, kind, payload):
        frame = GUEST_MAC + PEER_MAC + struct.pack('!H', kind) + payload
        self.endpoint.sendto(frame.ljust(60, b'\0'), self.address)

    def ip(self, protocol, data):
        header = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20 + len(data), 123, 0x4000, 64, protocol, 0, PEER_IP, GUEST_IP)
        header = header[:10] + struct.pack('!H', checksum(header)) + header[12:]
        self.ethernet(0x800, header + data)

    def udp(self, source, destination, data):
        packet = struct.pack('!HHHH', source, destination, 8 + len(data), 0) + data
        packet = packet[:6] + struct.pack('!H', checksum(pseudo(PEER_IP, GUEST_IP, 17, packet)) or 65535) + packet[8:]
        self.ip(17, packet)

    def tcp(self, source, destination, sequence, ack, flags, data=b''):
        packet = struct.pack('!HHIIBBHHH', source, destination, sequence, ack, 0x50, flags, 4096, 0, 0) + data
        packet = packet[:16] + struct.pack('!H', checksum(pseudo(PEER_IP, GUEST_IP, 6, packet))) + packet[18:]
        self.ip(6, packet)

    def receive(self, frame, address):
        self.address = address
        assert frame[6:12] == GUEST_MAC
        kind = struct.unpack('!H', frame[12:14])[0]
        if kind == 0x806:
            assert frame[14:22] == struct.pack('!HHBBH', 1, 0x800, 6, 4, 1)
            assert frame[28:32] == GUEST_IP and frame[38:42] == PEER_IP
            self.ethernet(0x806, struct.pack('!HHBBH', 1, 0x800, 6, 4, 2) + PEER_MAC + PEER_IP + GUEST_MAC + GUEST_IP)
            self.events.append('arp')
            return
        assert kind == 0x800 and frame[:6] == PEER_MAC
        ip = frame[14:]
        header_length = (ip[0] & 15) * 4
        total = struct.unpack('!H', ip[2:4])[0]
        assert checksum(ip[:header_length]) == 0 and ip[12:16] == GUEST_IP and ip[16:20] == PEER_IP
        payload = ip[header_length:total]
        if ip[9] == 1:
            assert checksum(payload) == 0 and payload[0] == 8
            reply = b'\0' + payload[1:2] + b'\0\0' + payload[4:]
            self.ip(1, reply[:2] + struct.pack('!H', checksum(reply)) + reply[4:])
            self.events.append('icmp')
        elif ip[9] == 17:
            assert checksum(pseudo(GUEST_IP, PEER_IP, 17, payload)) == 0
            source, destination, length, _ = struct.unpack('!HHHH', payload[:8])
            assert destination == 9000 and length == len(payload)
            if payload[8:] == b'udp-request':
                self.udp(9000, source, b'udp-response'); self.events.append('udp')
            elif payload[8:] == b'listen-ready':
                self.tcp(30000, 8080, 9000, 0, 2); self.events.append('listen')
            else: raise AssertionError(payload)
        elif ip[9] == 6:
            assert checksum(pseudo(GUEST_IP, PEER_IP, 6, payload)) == 0
            source, destination, sequence, ack, offset, flags, _, _, _ = struct.unpack('!HHIIBBHHH', payload[:20])
            data = payload[(offset >> 4) * 4:]
            if destination == 9001:
                self.active_port = source
                if flags == 2:
                    self.syns += 1
                    if self.syns > 1: self.tcp(9001, source, 5000, sequence + 1, 0x12)
                elif data:
                    assert data == b'tcp-request'; self.data_attempts += 1
                    if self.data_attempts > 1:
                        self.tcp(9001, source, 5001, sequence + len(data), 0x10)
                        self.tcp(9001, source, 5008, sequence + len(data), 0x18, b'wrong')
                        self.tcp(9001, source, 5001, sequence + len(data), 0x19, b'tcp-response')
                        self.events.append('tcp-retransmitted-data')
                elif flags & 1:
                    assert ack == self.active_next
                    self.tcp(9001, source, self.active_next, sequence + 1, 0x10)
                    self.events.append('tcp-client-fin')
            elif destination == 30000:
                assert source == 8080
                if flags == 0x12:
                    assert ack == 9001
                    self.tcp(30000, 8080, 9001, sequence + 1, 0x10)
                    self.tcp(30000, 8080, 9001, sequence + 1, 0x18, b'server-request')
                    self.passive_next = 9015
                elif data:
                    assert data == b'server-response'
                    self.tcp(30000, 8080, self.passive_next, sequence + len(data), 0x10)
                    self.tcp(30000, 8080, self.passive_next, sequence + len(data), 0x11)
                    self.passive_next += 1
                    self.events.append('tcp-server-data')
                elif flags & 1:
                    self.tcp(30000, 8080, self.passive_next, sequence + 1, 0x10)
                    self.events.append('tcp-server-fin')
            elif destination == 9010:
                assert flags == 2
                self.tcp(9010, source, 0, sequence + 1, 0x14); self.events.append('refused')
            elif destination == 9011:
                assert flags == 2
                self.timeout_syns += 1
            else: raise AssertionError((source, destination, flags, data))
        else: raise AssertionError(ip[9])


def run(firmware):
    output = REPO / 'build/tests' / ('network-' + firmware); output.mkdir(parents=True, exist_ok=True)
    serial = output / 'serial.log'; serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='agenticos-network-') as directory, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as endpoint:
        work = Path(directory); qmp_path = work / 'qmp.sock'
        endpoint.bind(('127.0.0.1', 0)); endpoint.setblocking(False)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserve:
            reserve.bind(('127.0.0.1', 0)); guest_port = reserve.getsockname()[1]
        peer = Peer(endpoint)
        iso = REPO / 'build/network-scenario/agenticos.iso'
        cmd = command(firmware, directory, headless=True, iso=iso)
        cmd += ['-netdev', f'socket,id=n0,udp=127.0.0.1:{endpoint.getsockname()[1]},localaddr=127.0.0.1:{guest_port}', '-device', 'e1000,netdev=n0,mac=52:54:00:12:34:56', '-serial', f'file:{serial}', '-qmp', f'unix:{qmp_path},server=on,wait=off']
        (output / 'command.json').write_text(json.dumps({'command': cmd, 'iso_sha256': hashlib.sha256(iso.read_bytes()).hexdigest()}, indent=2) + '\n')
        qmp = None
        with (output / 'qemu.log').open('wb') as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 40
                while time.monotonic() < deadline:
                    assert process.poll() is None, (output / 'qemu.log').read_text()
                    text = serial.read_text(errors='replace') if serial.exists() else ''
                    assert '[PANIC]' not in text and '[ERROR]' not in text, text[-5000:]
                    if qmp is None and qmp_path.exists(): qmp = QMP(qmp_path)
                    while True:
                        try: frame, address = endpoint.recvfrom(4096)
                        except BlockingIOError: break
                        peer.receive(frame, address)
                    if 'AGENTICOS: PHASE12 PASS' in text: break
                    time.sleep(0.005)
                else: raise TimeoutError(text[-5000:])
                assert 'AGENTICOS: NETWORK SOCKET PASS' in text and 'native network checks passed' in text
                assert peer.syns >= 2 and peer.data_attempts >= 2 and peer.timeout_syns == 4
                assert set(peer.events) >= {'arp', 'icmp', 'udp', 'listen', 'tcp-retransmitted-data', 'tcp-client-fin', 'tcp-server-data', 'tcp-server-fin', 'refused'}
                validate_diagnostics(text, 'normal', 'INFO')
                (output / 'results.json').write_text(json.dumps({'events': peer.events, 'syn_attempts': peer.syns, 'data_attempts': peer.data_attempts, 'timeout_syns': peer.timeout_syns}, indent=2) + '\n')
                registers = qmp.execute('human-monitor-command', {'command-line': 'info registers'})
                assert 'HLT=1' in registers and 'CPL=0' in registers
                (output / 'registers.txt').write_text(registers)
                qmp.execute('screendump', {'filename': str(output / 'screen.ppm')}); validate_screen(output / 'screen.ppm', output / 'screen.png')
                qmp.execute('quit'); assert process.wait(timeout=5) == 0
                print(f'PASS: {firmware.upper()} native sockets over real Ethernet: ARP/IPv4/ICMP/UDP, TCP client/server, retransmission, out-of-order rejection, FIN/EOF, refusal and timeout')
            finally:
                if qmp is not None: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)


if __name__ == '__main__':
    subprocess.run(['make', '-j2', 'BUILD=build/network-scenario', 'BOOT_TEST=network', 'iso', 'check-elf'], cwd=REPO, check=True)
    for firmware in ('bios', 'uefi'): run(firmware)
