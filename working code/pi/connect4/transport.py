"""One serial owner. Outbound commands never survive a reconnect."""
import json
import queue
import threading
import time


class SerialLink:
    def __init__(self, port, emit, factory=None):
        self.port, self.emit, self.factory = port, emit, factory
        self.outbound = queue.Queue(maxsize=16)
        self.lock = threading.Lock()
        self.connected = False
        self.stopped = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True, name="Serial")

    def start(self):
        self.thread.start()

    def send(self, line):
        if not line or len(line) > 126 or "\n" in line or "\r" in line:
            return False
        with self.lock:
            if not self.connected:
                return False
            try:
                self.outbound.put_nowait((line + "\n").encode("ascii"))
                return True
            except (queue.Full, UnicodeError):
                return False

    def _disconnect(self):
        with self.lock:
            self.connected = False
            while True:
                try:
                    self.outbound.get_nowait()
                except queue.Empty:
                    break
        self.emit({"type": "link", "connected": False})

    def _open(self):
        if self.factory:
            return self.factory()
        import serial
        link = serial.Serial(port=None, baudrate=115200, timeout=0.05,
                             write_timeout=0.2, exclusive=True)
        link.dtr, link.rts = False, False
        link.port = self.port
        link.open()
        return link

    def _session(self, link):
        buffer = bytearray()
        discarding = False
        with self.lock:
            self.connected = True
        self.emit({"type": "link", "connected": True})
        next_snapshot = 0
        while not self.stopped.is_set():
            now = time.monotonic()
            if now >= next_snapshot:
                link.write(b"snapshot\n")
                next_snapshot = now + 2
            try:
                command = self.outbound.get_nowait()
            except queue.Empty:
                command = None
            if command:
                if link.write(command) != len(command):
                    raise OSError("Incomplete serial write")
            for byte in link.read(4096):
                if byte == 10:
                    if not discarding:
                        try:
                            event = json.loads(buffer.decode("utf-8"))
                            if isinstance(event, dict) and event.get("v") == 1:
                                self.emit(event)
                        except (ValueError, UnicodeError):
                            pass  # ESP32 ROM boot text and incomplete reconnect frames.
                    buffer.clear()
                    discarding = False
                elif not discarding:
                    if len(buffer) >= 4096:
                        discarding = True
                        buffer.clear()
                    else:
                        buffer.append(byte)

    def _run(self):
        while not self.stopped.is_set():
            link = None
            try:
                link = self._open()
                self._session(link)
            except Exception:
                self.emit({"type": "notice", "text": "USB serial unavailable; reconnecting without replaying commands."})
            finally:
                self._disconnect()
                if link:
                    try:
                        link.close()
                    except OSError:
                        pass
            self.stopped.wait(2)

    def close(self):
        self.stopped.set()
        self.thread.join(timeout=1)
