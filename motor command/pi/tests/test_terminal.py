import unittest
from motor_terminal import MotorSession, validate_command


class FakePort:
    def __init__(self, incoming=b"MOTOR_TEST v1 READY\n"):
        self.input = bytearray(incoming)
        self.writes = []

    @property
    def in_waiting(self):
        return len(self.input)

    def read(self, size):
        result = bytes(self.input[:size])
        del self.input[:size]
        return result

    def write(self, data):
        self.writes.append(data)
        return len(data)


class TerminalTests(unittest.TestCase):
    def test_validation(self):
        self.assertEqual(validate_command(" servo  0 100 "), "servo 0 100")
        for command in ("servo 7 180", "off 0", "off all", "help", "status", "quit"):
            self.assertEqual(validate_command(command), command)
        for command in ("servo 8 90", "servo 0 181", "servo 0 -1", "servo 0 90 extra",
                        "servo 0 90\noff all", "servo 0 1.5", "ping", "servo 0 999999999999"):
            with self.assertRaises(ValueError):
                validate_command(command)

    def test_handshake_stop_and_no_automatic_servo(self):
        port = FakePort()
        session = MotorSession(port, emit=lambda _: None)
        session.handshake()
        self.assertEqual(port.writes, [b"info\n", b"off all\n"])
        session.command("servo 0 100")
        self.assertEqual(port.writes[-1], b"servo 0 100\n")
        self.assertFalse(session.command("quit"))
        self.assertEqual(port.writes[-1], b"off all\n")

    def test_heartbeat_and_fragmented_identity(self):
        now = [0.0]
        port = FakePort(b"MOTOR_TEST v1 ")
        session = MotorSession(port, emit=lambda _: None, clock=lambda: now[0])
        session.read()
        self.assertFalse(session.identified)
        port.input.extend(b"READY\r\n")
        session.read()
        self.assertTrue(session.identified)
        session.heartbeat()
        now[0] = 0.49
        session.heartbeat()
        self.assertEqual(port.writes, [b"ping\n"])
        now[0] = 0.5
        session.heartbeat()
        self.assertEqual(port.writes, [b"ping\n", b"ping\n"])

    def test_wrong_firmware_times_out_without_movement(self):
        now = [0.0]
        port = FakePort(b'{"type":"state"}\n')
        session = MotorSession(port, emit=lambda _: None, clock=lambda: now[0])
        def sleep(duration):
            now[0] += duration
        with self.assertRaises(RuntimeError):
            session.handshake(timeout=0.2, sleep=sleep)
        self.assertEqual(port.writes, [b"info\n"])

    def test_invalid_commands_and_partial_write(self):
        port = FakePort()
        session = MotorSession(port, emit=lambda _: None)
        with self.assertRaises(RuntimeError):
            session.command("servo 0 100")
        session.handshake()
        previous = list(port.writes)
        with self.assertRaises(ValueError):
            session.command("servo 0 181")
        self.assertEqual(port.writes, previous)
        port.write = lambda data: len(data)-1
        with self.assertRaises(OSError):
            session.command("servo 0 100")


if __name__ == "__main__":
    unittest.main()
