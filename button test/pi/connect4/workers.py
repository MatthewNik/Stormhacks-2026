"""Replace pending work instead of accumulating speech for old boards."""
from dataclasses import dataclass
from pathlib import Path
import hashlib
import subprocess
import threading
import time
import wave
from .coaching import evidence_for, fallback
from .providers import ProviderError

UNSAFE_PHASES = {"Fault", "Paused", "Stopped", "AwaitClear", "Correction", "ManualBaseline",
                 "ManualWait", "AwaitCorrection"}


def context_for(state):
    return state["boot_id"], state["game_id"], state["move_number"]


@dataclass(frozen=True)
class Task:
    token: int
    state: dict
    kind: str


class LatestWorker:
    def __init__(self, process, error, name):
        self.process, self.error = process, error
        self.condition = threading.Condition()
        self.pending = None
        self.stopped = False
        self.thread = threading.Thread(target=self._run, daemon=True, name=name)
        self.thread.start()

    def submit(self, item):
        with self.condition:
            self.pending = item
            self.condition.notify()

    def clear(self):
        with self.condition:
            self.pending = None

    def _run(self):
        while True:
            with self.condition:
                self.condition.wait_for(lambda: self.stopped or self.pending is not None)
                if self.stopped:
                    return
                item, self.pending = self.pending, None
            try:
                self.process(item)
            except Exception:
                # Provider/socket error bodies and credentials must not reach logs.
                self.error(f"{self.thread.name} failed; gameplay remains available.")

    def close(self):
        with self.condition:
            self.stopped, self.pending = True, None
            self.condition.notify()
        self.thread.join(timeout=0.5)


class AudioPlayer:
    def __init__(self, device="default"):
        self.device = device
        self.lock = threading.Lock()
        self.process = None

    def cancel(self):
        with self.lock:
            if self.process and self.process.poll() is None:
                self.process.terminate()

    def play(self, path, current):
        with self.lock:
            if not current():
                return
            self.process = subprocess.Popen(["aplay", "-q", "-D", self.device, str(path)],
                                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            process = self.process
        while process.poll() is None:
            if not current():
                self.cancel()
                break
            time.sleep(0.05)
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=1)
        if current() and process.returncode:
            raise RuntimeError("Audio playback failed")


class CoachService:
    def __init__(self, gemini, tts, config, cache, on_advice, on_error, player=None):
        self.gemini, self.tts, self.config = gemini, tts, config
        self.cache = Path(cache)
        self.cache.mkdir(parents=True, exist_ok=True)
        self.on_advice, self.on_error = on_advice, on_error
        self.player = player or AudioPlayer(config.audio_device)
        self.lock = threading.Lock()
        self.token = 0
        self.context = None
        self.last_task = None
        self.closed = False
        self.coach_worker = LatestWorker(self._coach, on_error, "Coaching")
        self.speech_worker = LatestWorker(self._speech, on_error, "Speech")

    def current(self, task):
        with self.lock:
            return not self.closed and task.token == self.token and context_for(task.state) == self.context

    def invalidate(self):
        with self.lock:
            self.token += 1
            self.context = None
            self.last_task = None
        self.coach_worker.clear()
        self.speech_worker.clear()
        self.player.cancel()

    def observe(self, state):
        if not state["game_id"] or state["phase"] in UNSAFE_PHASES:
            self.invalidate()
            return
        context = context_for(state)
        kind = None
        if state["phase"] == "Ended":
            kind = "review"
        elif state["mode"] == "coach":
            if state["phase"] == "HumanReady":
                kind = "hint"
            elif state["moves"] and state["moves"][-1]["symbol"] == state["human"]:
                kind = "feedback"
        with self.lock:
            changed = context != self.context
            if changed:
                self.token += 1
                self.context = context
                self.last_task = None
            identity = (context, kind)
            if kind and identity != self.last_task:
                self.last_task = identity
                task = Task(self.token, state, kind)
            else:
                task = None
        if changed:
            self.coach_worker.clear()
            self.speech_worker.clear()
            self.player.cancel()
        if task:
            self.coach_worker.submit(task)

    def _coach(self, task):
        if not self.current(task):
            return
        evidence, suggested = evidence_for(task.state, task.kind)
        if not self.current(task):
            return
        text, source = fallback(evidence, task.kind), "local"
        attempts = 2 if task.kind == "review" else 1
        for _ in range(attempts):
            if not self.current(task):
                return
            try:
                text = self.gemini.explain(task.state, evidence, task.kind, suggested)
                source = "gemini"
                break
            except ProviderError:
                if not self.config.gemini_key:
                    break
        if not self.current(task):
            return
        self.on_advice(task, text, source)
        if self.config.speech and self.config.elevenlabs_key and self.current(task):
            self.speech_worker.submit((task, text))

    def _speech(self, item):
        task, text = item
        if not self.current(task):
            return
        digest = hashlib.sha256((self.config.voice_id + self.config.tts_model + text).encode()).hexdigest()
        path = self.cache / (digest + ".wav")
        if not path.exists():
            pcm = self.tts.speak(text)
            if not self.current(task):
                return
            with wave.open(str(path), "wb") as audio:
                audio.setnchannels(1)
                audio.setsampwidth(2)
                audio.setframerate(24000)
                audio.writeframes(pcm)
            # Keep cache bounded to 128 explanations.
            for old in sorted(self.cache.glob("*.wav"), key=lambda p: p.stat().st_mtime, reverse=True)[128:]:
                old.unlink()
        if self.current(task):
            self.player.play(path, lambda: self.current(task))

    def close(self):
        self.invalidate()
        with self.lock:
            self.closed = True
        self.coach_worker.close()
        self.speech_worker.close()
