import json
from pathlib import Path
import tempfile
import threading
import time
import unittest
from unittest.mock import Mock, patch

from connect4 import board
from connect4.app import Application
from connect4.config import Config, output_path
from connect4.history import GameStore, validate_state
from connect4.providers import Gemini, ElevenLabs, ProviderError, validate_advice, post
from connect4.transport import SerialLink
from connect4.workers import CoachService, Task

TEST_ROOT = Path(__file__).resolve().parents[2] / "build" / "python-tests"
TEST_ROOT.mkdir(parents=True, exist_ok=True)


def state(columns=(), phase="HumanReady", mode="coach", boot=42, game=1, revision=1):
    moves = [{"column": c, "symbol": "O" if i % 2 == 0 else "X"} for i, c in enumerate(columns)]
    position = board.replay(moves)
    return {"v": 1, "type": "snapshot", "boot_id": boot, "game_id": game, "revision": revision,
            "move_number": len(moves), "mode": mode, "difficulty": 1, "robot_first": False,
            "human": "O", "robot": "X", "phase": phase, "pending_column": 0,
            "result": board.result(position), "dropped": 0, "board": list(position), "moves": moves}


class BoardTests(unittest.TestCase):
    def test_wins_blocks_and_immutable_board(self):
        for symbol in ("O", "X"):
            position = board.EMPTY
            for column in (1, 2, 3):
                position = board.drop(position, column, symbol)
            before = position
            self.assertEqual(board.winning_columns(position, symbol), [4])
            self.assertEqual(board.analyse(position, symbol)["recommended_column"], 4)
            self.assertEqual(board.analyse(position, board.other(symbol))["recommended_column"], 4)
            self.assertEqual(position, before)

    def test_gravity_full_column_and_draw(self):
        position = board.replay([{"column": 1, "symbol": "O" if i % 2 == 0 else "X"} for i in range(6)])
        self.assertNotIn(1, board.legal(position))
        with self.assertRaises(ValueError):
            board.drop(position, 1, "O")
        self.assertEqual(position[5][0], "O")
        draw = tuple("OOXXOOX" if r % 2 == 0 else "XXOOXXO" for r in range(6))
        self.assertEqual(board.result(draw), "draw")
        self.assertEqual(board.scores(draw, "O"), ())

    def test_win_directions_and_forks(self):
        for dr, dc in ((0, 1), (1, 0), (1, 1), (1, -1)):
            rows = [list(".......") for _ in range(6)]
            for i in range(4):
                rows[i * dr][3 + i * dc] = "O"
            self.assertEqual(board.result(tuple("".join(r) for r in rows)), "O_wins")
        position = board.EMPTY
        for c in (2, 3):
            position = board.drop(position, c, "O")
        self.assertIn(4, board.fork_columns(position, "O"))
        forked = board.drop(position, 4, "O")
        self.assertEqual(set(board.winning_columns(forked, "O")), {1, 5})

    def test_history_rejects_post_win_and_wrong_turn(self):
        with self.assertRaises(ValueError):
            board.replay([{"column": 4, "symbol": "X"}])
        with self.assertRaises(ValueError):
            state((1, 2, 1, 2, 1, 2, 1, 3))


class HistoryTests(unittest.TestCase):
    def test_reconnect_gap_duplicate_and_restart_persistence(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            store = GameStore(directory)
            self.assertTrue(store.accept(state()))
            self.assertTrue(store.accept(state((4, 3, 5, 3), revision=9)))
            self.assertFalse(store.accept(state((4, 3, 5, 3), revision=9)))
            self.assertFalse(store.accept(state((4, 3), revision=2)))
            store.disconnected()
            restored = GameStore(directory)
            restored.accept(state((4, 3, 5, 3), revision=9))
            records = [json.loads(line) for line in restored.path.read_text().splitlines()]
            self.assertEqual(len([r for r in records if r["type"] == "committed_move"]), 4)
            self.assertEqual(restored.status, "active")

    def test_reset_completion_and_clear_status(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            store = GameStore(directory)
            store.accept(state((1, 2), revision=2))
            old = store.path
            store.accept(state(boot=43, game=0, phase="AwaitClear"))
            self.assertIn('"status":"incomplete"', old.read_text())
            store.accept(state((1, 2, 1, 2, 1, 2, 1), phase="Ended", boot=43, revision=8))
            self.assertEqual(store.status, "completed")
            completed = store.path
            store.accept(state((1, 2, 1, 2, 1, 2, 1), phase="AwaitClear", boot=43, revision=9))
            self.assertEqual(store.status, "completed")
            self.assertIn('"status":"completed"', completed.read_text())

    def test_validation_and_conflicting_history(self):
        bad = state((4,))
        bad["board"][5] = "......."
        with self.assertRaises(ValueError):
            validate_state(bad)
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            store = GameStore(directory)
            store.accept(state((4,), revision=2))
            with self.assertRaises(ValueError):
                store.accept(state((5,), revision=3))

    def test_paths_cannot_escape_service(self):
        with self.assertRaises(ValueError):
            output_path("../../escape")


class ProviderTests(unittest.TestCase):
    def test_http_deadline_and_closed_connection_success(self):
        connection = Mock()
        connection.sock.fileno.side_effect = [1, 1, 1, -1, -1]
        connection.getresponse.return_value.status = 200
        connection.getresponse.return_value.read1.side_effect = [b"ok", b""]
        with patch("connect4.providers.http.client.HTTPSConnection", return_value=connection), patch(
                "connect4.providers.time.monotonic", side_effect=[0, 1, 2, 3, 4, 5]):
            self.assertEqual(post("example.invalid", "/", {}, {}, 100), b"ok")
        connection.close.assert_called_once()
        connection = Mock()
        connection.sock.fileno.return_value = 1
        connection.getresponse.return_value.status = 200
        connection.getresponse.return_value.read1.return_value = b"slow"
        with patch("connect4.providers.http.client.HTTPSConnection", return_value=connection), patch(
                "connect4.providers.time.monotonic", side_effect=[0, 1, 2, 3, 11]):
            with self.assertRaises(ProviderError):
                post("example.invalid", "/", {}, {}, 100)
        connection.close.assert_called_once()

    def test_http_failures_are_sanitized_and_responses_bounded(self):
        connection = Mock()
        connection.connect.side_effect = OSError("test-secret should never be logged")
        with patch("connect4.providers.http.client.HTTPSConnection", return_value=connection):
            with self.assertRaises(ProviderError) as error:
                post("example.invalid", "/", {}, {}, 100)
        self.assertNotIn("test-secret", str(error.exception))
        connection = Mock()
        connection.sock.fileno.return_value = 1
        connection.getresponse.return_value.status = 200
        connection.getresponse.return_value.read1.return_value = b"oversized"
        with patch("connect4.providers.http.client.HTTPSConnection", return_value=connection):
            with self.assertRaises(ProviderError):
                post("example.invalid", "/", {}, {}, 2)

    def test_structured_response_headers_and_constraints(self):
        answer = {"text": "Consider column 4.", "move_number": 0, "suggested_column": 4}
        response = {"candidates": [{"content": {"parts": [{"text": json.dumps(answer)}]}}]}
        request = Mock(return_value=json.dumps(response).encode())
        config = Config(gemini_key="test-secret")
        self.assertEqual(Gemini(config, request).explain(state(), {}, "hint", 4), answer["text"])
        args = request.call_args.args
        self.assertNotIn("test-secret", args[1])
        self.assertEqual(args[2]["x-goog-api-key"], "test-secret")
        self.assertIn("responseJsonSchema", args[3]["generationConfig"])
        for changed in ({"move_number": 1}, {"suggested_column": 7}, {"text": "One. Two. Three."},
                        {"text": "word " * 61}, {"suggested_column": True}):
            with self.assertRaises(ProviderError):
                validate_advice({**answer, **changed}, 0, 4)

    def test_missing_invalid_response_and_pcm(self):
        with self.assertRaises(ProviderError):
            Gemini(Config()).explain(state(), {}, "hint", 4)
        with self.assertRaises(ProviderError):
            Gemini(Config(gemini_key="test"), Mock(return_value=b"{}" )).explain(state(), {}, "hint", 4)
        request = Mock(return_value=b"\0\0" * 10)
        self.assertEqual(len(ElevenLabs(Config(elevenlabs_key="test"), request).speak("Hello")), 20)
        self.assertIn("output_format=pcm_24000", request.call_args.args[1])
        with self.assertRaises(ProviderError):
            ElevenLabs(Config(elevenlabs_key="test"), Mock(return_value=b"x")).speak("Hello")


class WorkerTests(unittest.TestCase):
    def test_stale_api_result_is_discarded_and_latest_pending_wins(self):
        started, release, delivered = threading.Event(), threading.Event(), threading.Event()
        seen = []

        class SlowGemini:
            def explain(self, current, evidence, kind, suggested):
                if current["move_number"] == 0:
                    started.set()
                    release.wait(3)
                return f"Advice for move {current['move_number']}."

        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            with patch("connect4.workers.evidence_for", return_value=({"current": {
                    "immediate_wins": [], "opponent_wins": [], "fork_columns": [], "recommended_column": 4}}, 4)):
                service = CoachService(SlowGemini(), Mock(), Config(gemini_key="test", speech=False), directory,
                                       lambda task, text, source: (seen.append(task.state["move_number"]), delivered.set()),
                                       lambda text: None, Mock())
                try:
                    service.observe(state())
                    self.assertTrue(started.wait(2))
                    service.observe(state((4, 3), revision=3))
                    service.observe(state((4, 3, 5, 3), revision=5))
                    release.set()
                    self.assertTrue(delivered.wait(3))
                    self.assertEqual(seen, [4])
                    service.observe(state((4, 3, 5, 3), phase="Fault", revision=6))
                    self.assertIsNone(service.context)
                finally:
                    release.set()
                    service.close()

    def test_api_failure_falls_back_and_review_retries_once(self):
        delivered = threading.Event()
        gemini = Mock()
        gemini.explain.side_effect = ProviderError("timeout")
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            service = CoachService(gemini, Mock(), Config(gemini_key="test", speech=False), directory,
                                   lambda task, text, source: delivered.set(), lambda text: None, Mock())
            try:
                service.observe(state((1, 2, 1, 2, 1, 2, 1), phase="Ended"))
                self.assertTrue(delivered.wait(15))
                self.assertEqual(gemini.explain.call_count, 2)
            finally:
                service.close()

    def test_tts_finishing_after_board_change_does_not_play(self):
        started, release = threading.Event(), threading.Event()
        tts = Mock()

        def synthesize(text):
            started.set()
            release.wait(2)
            return b"\0\0" * 10
        tts.speak.side_effect = synthesize
        player = Mock()
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            service = CoachService(Mock(), tts, Config(elevenlabs_key="test"), directory,
                                   lambda *args: None, lambda text: None, player)
            with service.lock:
                service.context = (42, 1, 0)
            task = Task(service.token, state(), "hint")
            thread = threading.Thread(target=service._speech, args=((task, "Hint"),))
            thread.start()
            self.assertTrue(started.wait(2))
            service.invalidate()
            release.set()
            thread.join(2)
            player.play.assert_not_called()
            service.close()


class TerminalTests(unittest.TestCase):
    def test_reconnect_logs_ir_moves_without_coaching_or_replay(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            coach, link = Mock(), Mock()
            app = Application(Config(), directory, coach, link, Mock())
            app.handle(state(mode="free"))
            for column in "1234567":
                app.command(column)
            app.handle({"type": "link", "connected": False})
            app.handle({"type": "link", "connected": True})
            app.handle(state((4, 3), mode="free", revision=3))
            app.handle(state((4, 3), mode="free", revision=3))
            link.send.assert_not_called()
            self.assertEqual(app.store.moves, state((4, 3))["moves"])
            self.assertFalse(coach.mock_calls)

    def test_default_startup_with_keys_never_constructs_services(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            with patch("connect4.providers.Gemini", side_effect=AssertionError("Gemini constructed")), patch(
                    "connect4.providers.ElevenLabs", side_effect=AssertionError("TTS constructed")), patch(
                    "connect4.workers.CoachService", side_effect=AssertionError("Coach constructed")), patch(
                    "socket.create_connection", side_effect=AssertionError("Network call")):
                app = Application(Config(gemini_key="test", elevenlabs_key="test", speech=True),
                                  directory, link=Mock(), printer=Mock())
                app.handle(state(mode="free"))
                app.handle(state((1, 2, 1, 2, 1, 2, 1), phase="Ended", mode="free", revision=8))
                app.command("coach")
                app.link.send.assert_not_called()
                self.assertFalse(any(p.name == "audio-cache" for p in Path(directory).iterdir()))

    def test_starter_and_control_commands_remain_available(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            link = Mock()
            app = Application(Config(), directory, link=link, printer=Mock())
            app.handle(state(phase="FirstPlayer", mode="free"))
            app.command("1")
            app.command("0")
            app.command("stop")
            app.command("diagnose")
            self.assertEqual([c.args[0] for c in link.send.call_args_list], ["1", "0", "stop", "diagnose"])

    def test_all_new_phases_validate(self):
        for phase in ("StartupPositioning", "HumanBaseline", "HumanConfirm", "IndexerReset"):
            self.assertEqual(validate_state(state(phase=phase))["phase"], phase)

    def test_pca_detail_prints_once_and_can_be_requested_again(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as directory:
            printer, link = Mock(), Mock()
            app = Application(Config(), directory, link=link, printer=printer)
            detail = {"type": "message", "text": "PCA detail: register 0xFE read failed"}
            for _ in range(4):
                app.handle(detail)
            printer.assert_called_once_with(detail["text"])
            app.command("diagnose")
            app.handle(detail)
            self.assertEqual(printer.call_count, 2)
            link.send.assert_called_once_with("diagnose")
            app.handle({"type": "link", "connected": True})
            app.handle(detail)
            self.assertEqual(sum(c.args == (detail["text"],) for c in printer.call_args_list), 3)
            app.handle({"type": "message", "text": "PCA detail: another error"})
            self.assertEqual(printer.call_args.args[0], "PCA detail: another error")


class TransportTests(unittest.TestCase):
    def test_silent_connection_notice_then_snapshot_recovery(self):
        events = []
        link = SerialLink("fake", events.append)
        frame = json.dumps(state()).encode() + b"\n"

        class Port:
            reads = 0

            def write(self, data):
                return len(data)

            def read(self, limit):
                self.reads += 1
                if self.reads == 2:
                    return frame
                if self.reads == 3:
                    link.stopped.set()
                return b""

        with patch("connect4.transport.time.monotonic", side_effect=[0, 0, 6, 12]):
            link._session(Port())
        notices = [e for e in events if e.get("type") == "notice"]
        self.assertEqual(len(notices), 1)
        self.assertIn("No ESP32 game snapshot", notices[0]["text"])
        self.assertIn(state(), events)

    def test_incomplete_snapshot_write_disconnects(self):
        link = SerialLink("fake", lambda event: None)
        port = Mock()
        port.write.return_value = 1
        with self.assertRaisesRegex(OSError, "Incomplete snapshot request"):
            link._session(port)
        port.read.assert_not_called()

    def test_fragmented_serial_boot_text_oversize_and_disconnect_queue(self):
        events = []
        link = SerialLink("fake", events.append)
        frame = json.dumps(state()).encode() + b"\n"

        class Port:
            writes = []
            chunks = iter([b"ESP32 boot\n" + b"x" * 4200 + b"\n" + frame[:30], frame[30:]])

            def write(self, data):
                self.writes.append(data)
                return len(data)

            def read(self, limit):
                try:
                    return next(self.chunks)
                except StopIteration:
                    link.stopped.set()
                    return b""

        port = Port()
        link._session(port)
        states = [event for event in events if event.get("type") == "snapshot"]
        self.assertEqual(states, [state()])
        self.assertEqual(port.writes, [b"snapshot\n"])
        self.assertTrue(link.send("stop"))
        link._disconnect()
        self.assertTrue(link.outbound.empty())
        self.assertFalse(link.send("4"))


if __name__ == "__main__":
    unittest.main()
