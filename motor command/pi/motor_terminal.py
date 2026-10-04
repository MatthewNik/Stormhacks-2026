"""Interactive USB terminal for the dedicated ESP32 motor-test firmware."""
import argparse
import queue
import re
import sys
import threading
import time

DEFAULT_PORT = "/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0"
HELP = "Commands: servo <0-7> <0-180>, off <0-7>, off all, status, help, quit"


def validate_command(text):
    parts = text.strip().split()
    if parts in (["help"], ["status"], ["quit"], ["off", "all"]):
        return " ".join(parts)
    if len(parts) == 2 and parts[0] == "off" and re.fullmatch(r"[0-7]", parts[1]):
        return " ".join(parts)
    if (len(parts) == 3 and parts[0] == "servo"
            and re.fullmatch(r"[0-7]", parts[1])
            and re.fullmatch(r"[0-9]{1,3}", parts[2])
            and 0 <= int(parts[2]) <= 180):
        return f"servo {int(parts[1])} {int(parts[2])}"
    raise ValueError(HELP)


class MotorSession:
    def __init__(self, port, emit=print, clock=time.monotonic):
        self.port, self.emit, self.clock = port, emit, clock
        self.buffer = bytearray()
        self.discarding = False
        self.identified = False
        self.last_ping = float("-inf")

    def write(self, text):
        payload = (text + "\n").encode("ascii")
        if self.port.write(payload) != len(payload):
            raise OSError("Incomplete serial write; stopped. No command will be replayed.")

    def read(self):
        # Bound work even when firmware or the wrong program floods serial output.
        data = self.port.read(min(self.port.in_waiting, 2048))
        for value in data:
            if value in (10, 13):
                if self.buffer and not self.discarding:
                    line = self.buffer.decode("utf-8", errors="replace")
                    if line in ("MOTOR_TEST v1 READY", "MOTOR_TEST v1 FAULT"):
                        self.identified = True
                    self.emit(line)
                self.buffer.clear()
                self.discarding = False
            elif not self.discarding:
                if len(self.buffer) < 512:
                    self.buffer.append(value)
                else:
                    self.buffer.clear()
                    self.discarding = True

    def handshake(self, timeout=8, sleep=time.sleep):
        deadline = self.clock() + timeout
        next_info = float("-inf")
        while self.clock() < deadline:
            if self.clock() >= next_info:
                self.write("info")
                next_info = self.clock() + 1
            self.read()
            if self.identified:
                # Establish a stopped session; never resume pulses from a prior operator.
                self.write("off all")
                return
            sleep(0.05)
        raise RuntimeError("Motor-test firmware was not identified. Flash MotorCommandTest first; close the game terminal.")

    def heartbeat(self):
        now = self.clock()
        if self.identified and now - self.last_ping >= 0.5:
            self.write("ping")
            self.last_ping = now

    def command(self, text):
        if not self.identified:
            raise RuntimeError("Motor-test firmware has not been identified.")
        command = validate_command(text)
        if command == "quit":
            self.write("off all")
            return False
        self.write(command)
        return True

    def stop(self):
        if self.identified:
            self.write("off all")


def input_lines(inbox):
    try:
        for line in sys.stdin:
            inbox.put(line.strip())
    finally:
        inbox.put(None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=DEFAULT_PORT)
    args = parser.parse_args()
    import serial
    port = None
    session = None
    try:
        # Disable DTR/RTS before opening; some USB boards may still reset on open.
        port = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=1, exclusive=True)
        port.dtr = False
        port.rts = False
        port.port = args.port
        port.open()
        port.reset_input_buffer()
        session = MotorSession(port)
        session.handshake()
        print(HELP, flush=True)
        print("Connected. Angles remain driven until off/quit or a 2-second heartbeat loss.", flush=True)
        inbox = queue.Queue()
        threading.Thread(target=input_lines, args=(inbox,), daemon=True).start()
        while True:
            session.read()
            session.heartbeat()
            try:
                line = inbox.get_nowait()
            except queue.Empty:
                time.sleep(0.02)
                continue
            if line is None:
                break
            if not line:
                continue
            try:
                if not session.command(line):
                    break
            except ValueError as error:
                print(error, flush=True)
    except KeyboardInterrupt:
        print("\nStopping motor outputs.", flush=True)
    except (serial.SerialException, OSError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr, flush=True)
        return 1
    finally:
        if session is not None:
            try:
                session.stop()
            except (serial.SerialException, OSError):
                pass  # ESP32 heartbeat expiry removes pulses if the USB link is lost.
        if port is not None:
            port.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
