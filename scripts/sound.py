import wave
import struct

INPUT_FILE = "out.csv"
OUTPUT_FILE = "output.wav"
SAMPLE_RATE = 16000

samples = []

with open(INPUT_FILE, "r") as f:
    for line in f:
        line = line.strip()
        if line:
            samples.append(int(line))

with wave.open(OUTPUT_FILE, "wb") as wav:
    wav.setnchannels(1)      # Mono
    wav.setsampwidth(2)      # 16 Bit
    wav.setframerate(SAMPLE_RATE)

    for sample in samples:
        # Sicherheit: auf int16 begrenzen
        sample = max(-32768, min(32767, sample))
        wav.writeframes(struct.pack("<h", sample))

print(f"WAV-Datei gespeichert als {OUTPUT_FILE}")