"""
Play a series of notes returned from our c files
thanks to https://www.johndcook.com/blog/2016/02/10/musical-pitch-notation/
"""

import numpy as np
import pyaudio

volume = 0.5
fs = 44100  # sample rate
duration = 1

input_str = " A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A3 E4 A4 A4 E4 A3 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 D5 D5 D5 D5 E5 E5 E5 E5 D#5 D#5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 C5 C5 C#5 C#5 D5 D5 D#5 D#5 E5 E5 F5 F5 F5 F5 F5 F5 F5 F5 F5 F5 F5 F5 E5 E5 E5 E5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 C5 C5 C#5 C#5 D5 D5 D#5 D#5 E5 E5 F5 F5 F5 F5 F5 F5 F5 F5 F5 F5 F5 F5 E5 E5 E5 E5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D5 D#5 D#5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 E5 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4 A4 A4 A4 A4 G4 G4 A4 A4 A#4 A#4 A4 A4 G4 G4 A4 A4"

notes = input_str.strip().split(" ")

A4 = 440
C0 = A4 * 2 ** -4.75

note_options = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_to_hz(note_num: str, stream) -> int:
    note_index = note_options.index(note_num[:-1])
    scale = int(note_num[-1])

    half_steps = scale * 12 + note_index

    hz = round((2 ** (half_steps / 12)) * C0)

    samples = (np.sin(2*np.pi*np.arange(fs*duration)*hz/fs)).astype(np.float32)

    stream.write(volume*samples)

    return hz


if __name__ == '__main__':
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paFloat32,
                    channels=1,
                    rate=fs,
                    output=True)
    try:
        for note in notes:
            note_to_hz(note, stream)
    finally:
        stream.stop_stream()
        stream.close()
        p.terminate()
