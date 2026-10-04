import argparse
from dataclasses import replace
import json
import queue
import secrets
import threading
from .config import Config, output_path
from .history import GameStore, STATE_TYPES
from .providers import Gemini, ElevenLabs
from .transport import SerialLink
from .workers import CoachService, context_for

COMMANDS = {"help", "board", "stop", "restart", "confirm-clear", "correct", "arm-manual",
            "confirm-correction", "free", "coach", "easy", "medium", "hard", "0", "1", "snapshot"}


class Application:
    def __init__(self, config, directory=None, coach=None, link=None, printer=print):
        self.events = queue.Queue(maxsize=256)
        self.store = GameStore(directory or output_path("logs"))
        self.print = printer
        self.synced = False
        self.pending = None
        self.pending_at = None
        self.request = secrets.randbits(32)
        self.running = True
        self.coach = coach or CoachService(Gemini(config), ElevenLabs(config), config,
                                           output_path("audio-cache"), self.advice_ready, self.notice)
        self.link = link or SerialLink(config.serial_port, self.emit)

    def emit(self, event):
        try:
            self.events.put_nowait(event)
        except queue.Full:
            # Periodic full snapshots repair missed telemetry.
            pass

    def notice(self, text):
        self.emit({"type": "notice", "text": text})

    def advice_ready(self, task, text, source):
        self.emit({"type": "advice_ready", "task": task, "text": text, "source": source})

    def handle(self, event):
        kind = event.get("type")
        if kind == "input":
            self.command(event["text"])
        elif kind == "link":
            self.synced = False
            self.pending = self.pending_at = None
            self.coach.invalidate()
            if not event["connected"]:
                self.store.disconnected()
                self.print("Disconnected. No pending move will be resent.")
            else:
                self.print("USB connected; waiting for an ESP32 snapshot.")
        elif kind in STATE_TYPES:
            was_synced = self.synced
            try:
                previous = self.store.state
                changed = self.store.accept(event)
            except (ValueError, OSError):
                self.synced = False
                self.coach.invalidate()
                self.print("State/log validation failed; human input disabled until a valid snapshot is available.")
                return
            self.synced = True
            state = self.store.state
            if self.pending_at is not None and context_for(state) != self.pending_at:
                self.pending = self.pending_at = None
            if changed or not was_synced:
                if previous is None or context_for(previous) != context_for(state) or previous["phase"] != state["phase"]:
                    self.print_state(state)
                self.coach.observe(state)
        elif kind == "command_result":
            self.print(f"Move request {event.get('request_id')}: {event.get('status')}")
            if event.get("request_id") == self.pending:
                self.pending = self.pending_at = None
        elif kind in ("message", "notice"):
            self.print(event.get("text", ""))
        elif kind == "advice_ready":
            task = event["task"]
            if self.synced and self.coach.current(task):
                try:
                    self.store.save_advice(context_for(task.state), event["text"], event["source"])
                except OSError:
                    self.print("Could not save coaching text.")
                self.print(f"Coach ({event['source']}): {event['text']}")

    def print_state(self, state):
        self.print(f"\n{state['mode']} | {state['phase']} | move {state['move_number']} | {state['result']}")
        self.print("  1 2 3 4 5 6 7")
        for row in state["board"]:
            self.print("| " + " ".join(row) + " |")
        if state["pending_column"]:
            self.print(f"Pending robot column: {state['pending_column']} (not committed)")
        if state["phase"] == "HumanReady":
            self.print("Enter the human's column 1-7 after placing the disc.")

    def command(self, text):
        text = text.strip()
        if text in ("quit", "exit"):
            self.running = False
            return
        state = self.store.state
        is_column = len(text) == 1 and text in "1234567"
        # 0/1 remain valid terminal starter selections on the starter screen.
        if is_column and not (state and state["phase"] == "FirstPlayer" and text == "1"):
            if not self.synced or not state or state["phase"] != "HumanReady":
                self.print("Human move unavailable; wait for a synchronized HumanReady state.")
                return
            if self.pending is not None:
                self.print("Previous move is unresolved. Use snapshot; do not repeat the physical move.")
                return
            self.request = (self.request + 1) & 0xFFFFFFFF
            tag = context_for(state)
            line = f"move {tag[0]} {tag[1]} {tag[2]} {self.request} {text}"
            if self.link.send(line):
                self.pending, self.pending_at = self.request, tag
            else:
                self.print("Move was not queued; USB is unavailable.")
        elif text in COMMANDS:
            if not self.link.send(text):
                self.print("Command was not queued; USB is unavailable.")
            elif text == "snapshot" and self.pending is not None:
                self.link.send(f"status {self.pending}")
        elif text:
            self.print("Enter 1-7, a documented ESP32 command, or quit. Use help for commands.")

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
        self.print("Pi terminal: confirm-clear, then buttons or free/coach. Type quit to leave.")
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
            self.coach.close()
            self.store.disconnected()


def replay_file(path):
    from .history import validate_state
    from .coaching import evidence_for, fallback
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
    if state["phase"] == "Ended":
        evidence, _ = evidence_for(state, "review")
        print(fallback(evidence, "review"))
    else:
        print("Game is unfinished; no completed-game review generated.")


def main():
    parser = argparse.ArgumentParser(description="Connect Four USB terminal and coaching sidecar")
    parser.add_argument("--serial-port")
    parser.add_argument("--no-speech", action="store_true")
    parser.add_argument("--local-only", action="store_true", help="No API requests")
    parser.add_argument("--replay", help="Read a saved JSONL game locally, without serial or APIs")
    args = parser.parse_args()
    if args.replay:
        replay_file(args.replay)
        return
    config = Config.load()
    if args.serial_port:
        config = replace(config, serial_port=args.serial_port)
    if args.no_speech:
        config = replace(config, speech=False)
    if args.local_only:
        config = replace(config, gemini_key="", elevenlabs_key="")
    Application(config).run()
