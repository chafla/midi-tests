"""
Play a series of notes returned from our c files
thanks to https://www.johndcook.com/blog/2016/02/10/musical-pitch-notation/
"""

import winsound
import time

input_str = "A#3 A#3 F4 F4 D#4 D#4 F4 F4 A#4 A#4 A#4 A#4 A#3 A#3 F4 F4 D#4 D#4 F4 F4 A#3 D4 D#4 D#4 D#4 D#4 F4 F4 F4 F4 F4 F4 A#3 A#3 F4 F4 D#4 D#4 F4 F4 A#4 A#4 A#4 A#4 A#3 A#3 F4 F4 D#4 D#4 F4 F4 A#3 A#3 A#3 A#3 C4 C4 C4 C4 D4 D4 D4 D4 D#4 D#4 D#4 D#4 F4 F4 F4 F4 F4 F4 A#3 A#3 F4 F4 D#4 D#4 F4 F4 A#4 A#4 A#4 A#4 A#3 A#3 F4 F4 D#4 D#4 F4 F4 A#3 D4 D#4 D#4 D#4 D#4 F4 F4 F4 F4 F4 F4"

notes = input_str.split(" ")

A4 = 440
C0 = A4 * 2 ** -4.75

note_options = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_to_hz(note_num: str) -> int:
    note_index = note_options.index(note_num[:-1])
    scale = int(note_num[-1])

    half_steps = scale * 12 + note_index

    hz = round((2 ** (half_steps / 12)) * C0)
    return hz


if __name__ == '__main__':
    for note in notes:
        # winsound.Beep(note_to_hz(note), 1)
        winsound.Beep(note_to_hz(note), 299)
        time.sleep(0.1)
