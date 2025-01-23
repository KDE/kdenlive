#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#pip3 install vosk
#pip3 install srt

from vosk import Model, KaldiRecognizer, SetLogLevel
import sys
import os
import subprocess
import argparse

SetLogLevel(-1)


def main():

    parser = argparse.ArgumentParser("VOSK to text script")
    parser.add_argument("-S", "--src", help="source audio file")
    parser.add_argument("-M", "--model", help="model name")
    parser.add_argument("-D", "--model_directory", help="the folder where the model is")
    parser.add_argument("-I", "--in_point", help="in point if not starting from 0", default="0")
    parser.add_argument("-O", "--out_point", help="out point if not operating on full file", default="0")
    parser.add_argument("-F", "--ffmpeg_path", help="path for ffmpeg", default="ffmpeg")
    args = parser.parse_args()

    src = args.src

    if src is None:
        config = vars(args)
        print(config)
        sys.exit()

    source = src.replace('"', '')
    print(f"ANALYSING SOURCE FILE: {source}.")
    if not os.path.exists(source):
        print(f"Source file does not exist: {source}.")
        sys.exit()

    model = args.model
    ffmpeg_path = args.ffmpeg_path

    os.chdir(args.model_directory)

    if not os.path.exists(model):
        print(f"Please download the model from https://alphacephei.com/vosk/models and unpack as {model} in the current folder.")
        exit (1)

    sample_rate=16000
    voskModel = Model(model)
    rec = KaldiRecognizer(voskModel, sample_rate)
    rec.SetWords(True)

    # zone rendering
    if (float(args.in_point)>0 or float(args.out_point)>0):
        process = subprocess.Popen([ffmpeg_path, '-loglevel', 'quiet', '-i',
                            source, '-ss', args.in_point, '-t', args.out_point,
                            '-ar', str(sample_rate), '-ac', '1', '-f', 's16le', '-'],
                            stdout=subprocess.PIPE)
    else:
        process = subprocess.Popen([ffmpeg_path, '-loglevel', 'quiet', '-i',
                            source,
                            '-ar', str(sample_rate), '-ac', '1', '-f', 's16le', '-'],
                            stdout=subprocess.PIPE)
    WORDS_PER_LINE = 7

    def transcribe():
        while True:
            data = process.stdout.read(4000)
            if len(data) == 0:
                sys.stdout.buffer.write(rec.FinalResult().encode('utf-8'))
                sys.stdout.flush()
                break
            if rec.AcceptWaveform(data):
                sys.stdout.buffer.write(rec.Result().encode('utf-8'))
                sys.stdout.flush()

    transcribe()


if __name__ == "__main__":
    sys.exit(main())
