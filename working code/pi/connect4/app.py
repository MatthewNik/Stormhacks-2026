import argparse
from dataclasses import replace
import json
import queue
import threading
from .config import Config, output_path
from .history import GameStore, STATE_TYPES
from .transport import SerialLink


def context_for(state):
    return state["boot_id"], state["game_id"], state["move_number"]

COMMANDS = {"help", "board", "stop", "restart", "confirm-clear", "correct", "arm-manual",
            "confirm-correction", "free", "coach", "easy", "medium", "hard", "0", "1", "snapshot", "diagnose"}


class Application:
    def __init__(self, config, directory=None, coach=None, link=None, printer=print):
        self.events = queue.Queue(maxsize=256)
        self.store = GameStore(directory or output_path("logs"))
        self.print = printer
        self.synced = False
        self.last_pca_detail = None
        self.running = True
        self.link = link or SerialLink(config.serial_port, self.emit)

    def emit(self, event):
        try:
            self.events.put_nowait(event)
        except queue.Full:
            # Periodic full snapshots repair missed telemetry.
            pass

    def handle(self, event):
        kind = event.get("type")
        if kind == "input":
            self.command(event["text"])
        elif kind == "link":
            self.synced = False
            self.last_pca_detail = None
            if not event["connected"]:
                self.store.disconnected()
                self.print("Disconnected. ESP32 owns gameplay; no moves will be resent.")
            else:
                self.print("USB connected; waiting for an ESP32 snapshot.")
        elif kind in STATE_TYPES:
            was_synced = self.synced
            try:
                previous = self.store.state
                changed = self.store.accept(event)
            except (ValueError, OSError):
                self.synced = False
                self.print("State/log validation failed; waiting for a valid snapshot. ESP32 still owns gameplay.")
                return
            self.synced = True
            state = self.store.state
            if previous and previous["boot_id"] != state["boot_id"]:
                self.last_pca_detail = None
            if changed or not was_synced:
                if previous is None or context_for(previous) != context_for(state) or previous["phase"] != state["phase"]:
                    self.print_state(state)
        elif kind == "command_result":
            self.print(f"Move request {event.get('request_id')}: {event.get('status')}")
        elif kind in ("message", "notice"):
            text = event.get("text", "")
            if text.startswith("PCA detail:"):
                if text == self.last_pca_detail:
                    return
                self.last_pca_detail = text
            self.print(text)

    def print_state(self, state):
        self.print(f"\n{state['mode']} | {state['phase']} | move {state['move_number']} | {state['result']}")
        self.print("  1 2 3 4 5 6 7")
        for row in state["board"]:
            self.print("| " + " ".join(row) + " |")
        if state["pending_column"]:
            self.print(f"Pending robot column: {state['pending_column']} (not committed)")
        if state["phase"] == "HumanReady":
            self.print("Insert ONE human disc; ESP32 IR sensors register its column automatically.")

    def command(self, text):
        text = text.strip()
        if text in ("quit", "exit"):
            self.running = False
            return
        state = self.store.state
        is_column = len(text) == 1 and text in "1234567"
        # 0/1 remain valid terminal starter selections on the starter screen.
        if is_column and not (state and state["phase"] == "FirstPlayer" and text == "1"):
            self.print("Human moves are sensor-only. Insert one disc when the OLED says ready.")
        elif text == "coach":
            self.print("Coach unavailable; select Free Play.")
        elif text in COMMANDS:
            if text == "diagnose":
                self.last_pca_detail = None
            if not self.link.send(text):
                self.print("Command was not queued; USB is unavailable.")
        elif text:
            self.print("Enter a documented ESP32 command or quit. Human moves use IR only; Coach is unavailable.")

    def run(self):
        self.link.start()

        def terminal():
            while self.running:
                try:
                    self.emit({"type": "input", "text": input()})
                except EOFError:
                    self.emit({"type": "input", "text": "quit"})
                    return
        threading.Thread(target=terminal, daemon=True, name="Terminal").start()
        self.print("Pi terminal: startup opens doors and loads motor 7, then opens Mode. Select Free Play; Coach unavailable. Human moves use IR. Type quit to leave.")
        try:
            while self.running:
                try:
                    self.handle(self.events.get(timeout=0.2))
                except queue.Empty:
                    continue
        except KeyboardInterrupt:
            pass
        finally:
            self.link.close()
            self.store.disconnected()


def replay_file(path):
    from .history import validate_state
    states = []
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            event = json.loads(line)
            if event.get("type") == "esp32_state":
                states.append(validate_state(event["state"]))
    if not states:
        raise ValueError("No ESP32 states found in log")
    state = states[-1]
    print(f"Game {state['game_id']}: {state['phase']}, {state['move_number']} committed moves")
    for row in state["board"]:
        print("| " + " ".join(row) + " |")


def main():
    parser = argparse.ArgumentParser(description="Connect Four USB terminal and game logs; IR moves, no APIs")
    parser.add_argument("--serial-port")
    parser.add_argument("--no-speech", action="store_true", help="Compatibility option; speech is always disabled")
    parser.add_argument("--local-only", action="store_true", help="Compatibility option; API calls are always disabled")
    parser.add_argument("--replay", help="Read a saved JSONL game locally, without serial or APIs")
    args = parser.parse_args()
    if args.replay:
        replay_file(args.replay)
        return
    config = Config.load()
    if args.serial_port:
        config = replace(config, serial_port=args.serial_port)
    Application(config).run()
