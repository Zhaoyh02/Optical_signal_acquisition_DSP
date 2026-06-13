#!/usr/bin/env python3
"""
Optical OOK Transceiver Test Suite
Connects to STM32F407 via USART1 (115200 bps) using CMIS register protocol.

Usage:
  python optical_transceiver_test.py COM3 --monitor
  python optical_transceiver_test.py COM3 --loopback
  python optical_transceiver_test.py COM3 --send "Hello"
  python optical_transceiver_test.py COM3 --ber 100
"""

import serial
import struct
import time
import sys


class CMIS_Client:
    def __init__(self, port: str, baudrate: int = 115200):
        self.ser = serial.Serial(port, baudrate, timeout=1.0)
        time.sleep(0.5)

    def read_reg(self, page: int, addr: int) -> int:
        cmd = f"R{page:02X}{addr:02X}\r\n"
        self.ser.write(cmd.encode())
        resp = self.ser.readline().decode(errors='ignore').strip()
        if resp.startswith('+'):
            return int(resp[1:], 16)
        if not resp:
            raise IOError(f"No response (timeout). Check: STM32 powered? Firmware flashed? TX/RX wired correctly?")
        raise IOError(f"Read error: {resp}")

    def write_reg(self, page: int, addr: int, data: int) -> None:
        cmd = f"W{page:02X}{addr:02X}{data:02X}\r\n"
        self.ser.write(cmd.encode())
        resp = self.ser.readline().decode(errors='ignore').strip()
        if resp != '+':
            raise IOError(f"Write error: {resp}")

    def read_u16(self, page: int, addr_msb: int) -> int:
        msb = self.read_reg(page, addr_msb)
        lsb = self.read_reg(page, addr_msb + 1)
        return (msb << 8) | lsb

    def read_s16(self, page: int, addr_msb: int) -> int:
        raw = self.read_u16(page, addr_msb)
        if raw & 0x8000:
            raw -= 0x10000
        return raw

    def read_temperature(self) -> float:
        raw = self.read_s16(0x00, 0x80)
        return raw / 256.0

    def read_vcc(self) -> float:
        raw = self.read_u16(0x00, 0x82)
        return raw * 100e-6

    def read_tx_power(self) -> float:
        raw = self.read_u16(0x00, 0x86)
        return raw * 0.1e-3

    def read_rx_power(self) -> float:
        raw = self.read_u16(0x00, 0x88)
        return raw * 0.1e-3

    def read_status(self) -> int:
        return self.read_reg(0x00, 0x8E)

    def enable_laser(self, tx_on: bool, rx_on: bool = False):
        val = (0x01 if tx_on else 0x00) | (0x02 if rx_on else 0x00)
        self.write_reg(0x01, 0x90, val)

    def enable_rx(self, on: bool):
        val = self.read_reg(0x01, 0x90)
        if on:
            val |= 0x02
        else:
            val &= ~0x02
        self.write_reg(0x01, 0x90, val)

    def send_frame(self, payload: bytes) -> None:
        if len(payload) > 255:
            raise ValueError("Payload too long (>255 bytes)")
        self.write_reg(0x01, 0xA0, len(payload))
        for i, byte in enumerate(payload):
            self.write_reg(0x01, 0xA2 + i, byte)
        self.write_reg(0x01, 0xA1, 0x01)

    def receive_frame(self):
        status = self.read_status()
        if status & 0x10:
            length = self.read_reg(0x01, 0xA0)
            data = bytes(self.read_reg(0x01, 0xA2 + i) for i in range(length))
            return data
        return None

    def monitor_ddm(self, duration_s: float = 10.0, interval_s: float = 1.0):
        print(f"{'Time':>6s} {'Temp(C)':>8s} {'VCC(V)':>8s} "
              f"{'TX(mW)':>8s} {'RX(mW)':>8s} {'Status':>8s}")
        start = time.time()
        while time.time() - start < duration_s:
            temp = self.read_temperature()
            vcc = self.read_vcc()
            tx = self.read_tx_power()
            rx = self.read_rx_power()
            status = self.read_status()
            elapsed = time.time() - start
            print(f"{elapsed:6.1f} {temp:8.2f} {vcc:8.3f} "
                  f"{tx:8.4f} {rx:8.4f} 0x{status:02X}")
            time.sleep(interval_s)

    def close(self):
        self.ser.close()


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Optical OOK Transceiver Test')
    parser.add_argument('port', help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--monitor', action='store_true', help='DDM monitoring mode')
    parser.add_argument('--loopback', action='store_true', help='Run loopback test')
    parser.add_argument('--send', type=str, help='Send text payload (requires --rx too)')
    parser.add_argument('--rx', action='store_true', help='Enable RX before sending')
    parser.add_argument('--laser', type=str, choices=['on','off'], help='Turn laser ON or OFF')
    args = parser.parse_args()

    client = CMIS_Client(args.port)
    try:
        module_id = client.read_reg(0, 0)
        print(f"Module ID: 0x{module_id:02X}")
        print(f"Temperature: {client.read_temperature():.1f} C")
        print(f"VCC: {client.read_vcc():.3f} V")

        if args.laser:
            on = (args.laser == 'on')
            client.enable_laser(tx_on=on)
            print(f"Laser {'ON' if on else 'OFF'}")
        elif args.monitor:
            client.monitor_ddm(duration_s=30)
        elif args.loopback or (args.send and args.rx):
            print("Enabling TX laser + RX...")
            client.enable_laser(tx_on=True, rx_on=True)
            time.sleep(0.2)

            test_msg = args.send.encode() if args.send else b"Hello Optical World!"
            print(f"Sending: {test_msg.decode()}")
            client.send_frame(test_msg)

            print("Waiting for loopback frame...")
            timeout = time.time() + 5.0
            while time.time() < timeout:
                received = client.receive_frame()
                if received:
                    try:
                        text = received.decode()
                    except UnicodeDecodeError:
                        text = str(received)
                    print(f"Received: {text}")
                    print(f"Match: {received == test_msg}")
                    break
                time.sleep(0.1)
            else:
                print("Timeout: no frame received")

            client.enable_laser(tx_on=False)
            client.enable_rx(False)
        elif args.send:
            client.enable_laser(tx_on=True)
            time.sleep(0.1)
            print(f"Sending: {args.send}")
            client.send_frame(args.send.encode())
            time.sleep(1)
            client.enable_laser(tx_on=False)
        else:
            print("No action specified. Use --monitor, --loopback, or --send.")
            print("Example: python optical_transceiver_test.py COM3 --monitor")
            print("Example: python optical_transceiver_test.py COM3 --loopback")
    finally:
        client.close()


if __name__ == '__main__':
    main()
