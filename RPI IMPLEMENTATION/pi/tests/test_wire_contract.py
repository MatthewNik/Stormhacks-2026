"""Exercise real C++ serialization through the production Python validator."""
import json
from pathlib import Path
import subprocess
import unittest
from connect4.history import validate_state
from connect4.board import analyse

EXECUTABLE = Path(__file__).resolve().parents[2] / "build/native/sidecar_tests.exe"


class WireContractTests(unittest.TestCase):
    @unittest.skipUnless(EXECUTABLE.exists(), "Run firmware native tests first on Windows")
    def test_cpp_frames_round_trip_and_board_analysis(self):
        result = subprocess.run([str(EXECUTABLE), "--frames"], check=True, capture_output=True, text=True)
        states = [validate_state(json.loads(line)) for line in result.stdout.splitlines()]
        self.assertEqual([s["move_number"] for s in states], [0, 1, 2])
        self.assertEqual(states[1]["moves"], [{"column": 4, "symbol": "O"}])
        self.assertEqual(states[1]["phase"], "IndexerRelease")
        self.assertEqual(states[2]["phase"], "HumanReady")
        tactical = analyse(tuple(states[2]["board"]), states[2]["human"])
        self.assertIn(tactical["recommended_column"], tactical["legal_columns"])
