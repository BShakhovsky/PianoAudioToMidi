# Polyphonic Piano Audio to MIDI Transcription

Generates piano MIDI-files from audio (MP3, WAV, etc.). The accuracy depends on the complexity of the song, and is obviously higher for solo piano pieces.  No instrument information is extracted, and all transcibed notes get combined into one part.

# Algorithm

Python version:

https://github.com/BShakhovsky/PolyphonicPianoTranscription

Here, at first I will just copy the code from above and will use Python embedded interpreter.  Then, step by step, I will try to translate it into pure C++, so that I will be able to call this module from my 3D-piano application:

https://github.com/BShakhovsky/PianoFingers3D