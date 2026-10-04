"""Bounded REST calls; credentials never appear in URLs or exceptions."""
import http.client
import json
import re
import time
from urllib.parse import quote


class ProviderError(Exception):
    pass


def post(host, path, headers, payload, max_bytes, timeout=10):
    deadline = time.monotonic() + timeout
    connection = http.client.HTTPSConnection(host, timeout=timeout)
    transport_socket = None

    def remaining():
        seconds = deadline - time.monotonic()
        if seconds <= 0:
            raise TimeoutError()
        if transport_socket is not None and transport_socket.fileno() >= 0:
            transport_socket.settimeout(seconds)

    try:
        connection.connect()
        transport_socket = connection.sock
        remaining()
        connection.request("POST", path, body=json.dumps(payload).encode(),
                           headers={"Content-Type": "application/json", **headers})
        remaining()
        response = connection.getresponse()
        if response.status != 200:
            raise ProviderError(f"Provider returned HTTP {response.status}")
        chunks, size = [], 0
        while True:
            remaining()
            chunk = response.read1(min(65536, max_bytes + 1 - size))
            if not chunk:
                break
            size += len(chunk)
            if size > max_bytes:
                raise ProviderError("Provider response exceeded size limit")
            chunks.append(chunk)
        remaining()
        return b"".join(chunks)
    except ProviderError:
        raise
    except (OSError, http.client.HTTPException, TimeoutError):
        raise ProviderError("Provider connection failed or timed out") from None
    finally:
        connection.close()


def validate_advice(data, move_number, suggested, review=False):
    if not isinstance(data, dict) or set(data) != {"text", "move_number", "suggested_column"}:
        raise ProviderError("Invalid coaching response fields")
    if type(data["move_number"]) is not int or data["move_number"] != move_number:
        raise ProviderError("Coaching response references an outdated move")
    column = data["suggested_column"]
    if column is not None and (type(column) is not int or column != suggested or review):
        raise ProviderError("Coaching response contains an unsupported recommendation")
    text = data["text"]
    if not isinstance(text, str) or not text.strip() or len(text) > 1600:
        raise ProviderError("Invalid coaching text")
    if any(ord(c) < 32 and c not in "\n\t" for c in text):
        raise ProviderError("Invalid coaching text")
    text = " ".join(text.split())
    if len(text.split()) > (120 if review else 60):
        raise ProviderError("Coaching response is too long")
    if not review and len([s for s in re.split(r"[.!?]+(?:\s|$)", text) if s.strip()]) > 2:
        raise ProviderError("Live coaching exceeds two sentences")
    return text


class Gemini:
    def __init__(self, config, request=post):
        self.config, self.request = config, request

    def explain(self, state, evidence, kind, suggested):
        if not self.config.gemini_key:
            raise ProviderError("Gemini key is not configured")
        review = kind == "review"
        instruction = (
            "You coach a human playing Connect Four. Explain only the supplied computed facts. "
            "Columns are 1-7 and row 0 is the top. Depth-5 scores are estimates, not perfect play. "
            "Do not invent wins, blocks, forks, board positions, or criticism unsupported by evidence. "
            "Teach centre control, immediate wins, blocking, and verified forks. "
            "Return JSON with text, move_number, and suggested_column. "
            "Use the supplied move_number exactly. suggested_column must be null or the supplied suggested column. "
        )
        instruction += (
            "In at most 120 words mention one evidenced good decision and up to two evidenced improvements; "
            "if none exist say so. suggested_column must be null."
            if review else "Use at most two short sentences and 60 words. For hint include feedback on the latest "
            "human move if supplied, then give a hint for the current board. For feedback explain the played move."
        )
        schema = {"type": "object", "properties": {
            "text": {"type": "string"}, "move_number": {"type": "integer"},
            "suggested_column": {"type": ["integer", "null"]}},
            "required": ["text", "move_number", "suggested_column"], "additionalProperties": False}
        context = {"kind": kind, "move_number": state["move_number"], "mode": state["mode"],
                   "human": state["human"], "robot": state["robot"], "board": state["board"],
                   "moves": state["moves"], "result": state["result"], "evidence": evidence,
                   "suggested_column": suggested}
        payload = {"systemInstruction": {"parts": [{"text": instruction}]},
                   "contents": [{"role": "user", "parts": [{"text": json.dumps(context)}]}],
                   "generationConfig": {"temperature": 0.2, "maxOutputTokens": 512,
                                        "responseMimeType": "application/json", "responseJsonSchema": schema}}
        raw = self.request("generativelanguage.googleapis.com",
                           "/v1beta/models/" + quote(self.config.gemini_model, safe="") + ":generateContent",
                           {"x-goog-api-key": self.config.gemini_key}, payload, 65536)
        try:
            response = json.loads(raw)
            parts = response["candidates"][0]["content"]["parts"]
            text = "".join(p["text"] for p in parts if "text" in p and not p.get("thought"))
            data = json.loads(text)
        except (ValueError, KeyError, IndexError, TypeError):
            raise ProviderError("Gemini returned no valid structured explanation") from None
        return validate_advice(data, state["move_number"], suggested, review)


class ElevenLabs:
    def __init__(self, config, request=post):
        self.config, self.request = config, request

    def speak(self, text):
        if not self.config.elevenlabs_key:
            raise ProviderError("ElevenLabs key is not configured")
        audio = self.request("api.elevenlabs.io",
                             "/v1/text-to-speech/" + quote(self.config.voice_id, safe="") + "?output_format=pcm_24000",
                             {"xi-api-key": self.config.elevenlabs_key},
                             {"text": text, "model_id": self.config.tts_model}, 4_000_000)
        if not audio or len(audio) % 2:
            raise ProviderError("ElevenLabs returned invalid PCM audio")
        return audio
