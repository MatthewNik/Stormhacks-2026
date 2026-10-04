from dataclasses import dataclass
from pathlib import Path
import os

ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Config:
    serial_port: str = "/dev/ttyUSB0"
    gemini_key: str = ""
    elevenlabs_key: str = ""
    gemini_model: str = "gemini-3.5-flash-lite"
    voice_id: str = "JBFqnCBsd6RMkjVDRZzb"
    tts_model: str = "eleven_flash_v2_5"
    audio_device: str = "default"
    speech: bool = True

    @classmethod
    def load(cls):
        from dotenv import load_dotenv
        load_dotenv(ROOT / ".env")
        return cls(serial_port=os.getenv("SERIAL_PORT", "/dev/ttyUSB0"),
                   gemini_key=os.getenv("GEMINI_API_KEY", ""),
                   elevenlabs_key=os.getenv("ELEVENLABS_API_KEY", ""),
                   gemini_model=os.getenv("GEMINI_MODEL", cls.gemini_model),
                   voice_id=os.getenv("ELEVENLABS_VOICE_ID", cls.voice_id),
                   tts_model=os.getenv("ELEVENLABS_MODEL", cls.tts_model),
                   audio_device=os.getenv("AUDIO_DEVICE", "default"),
                   speech=os.getenv("SPEECH_ENABLED", "true").lower() == "true")


def output_path(relative):
    path = (ROOT / relative).resolve()
    if not path.is_relative_to(ROOT):
        raise ValueError("Output path must remain inside the Pi service directory")
    return path
