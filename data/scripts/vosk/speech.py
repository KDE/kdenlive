#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

from vosk import Model, KaldiRecognizer, SetLogLevel
import sys
import os
import subprocess
import srt
import json
import datetime
import argparse

SetLogLevel(-1)


def main():
    parser = argparse.ArgumentParser("VOSK to text script")
    parser.add_argument("-S", "--src", help="source audio file")
    parser.add_argument("-M", "--model", help="model name")
    parser.add_argument("-D", "--model_directory", help="the folder where the model is")
    parser.add_argument("-I", "--in_point", help="in point if not starting from 0", default="0")
    parser.add_argument("-O", "--out_point", help="out point if not operating on full file", default="0")
    parser.add_argument("-F", "--ffmpeg_path", help="path for ffmpeg")
    parser.add_argument("--output", help="output file")
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
        exit(1)

    sample_rate = 16000
    voskModel = Model(model)
    rec = KaldiRecognizer(voskModel, sample_rate)
    rec.SetWords(True)

    process = subprocess.Popen([ffmpeg_path, '-loglevel', 'quiet', '-i', source,
                '-ar', str(sample_rate), '-ac', '1', '-f', 's16le', '-'], stdout=subprocess.PIPE)
    WORDS_PER_LINE = 7

    def transcribe():
        results = []
        subs = []
        progress = 0
        while True:
            data = process.stdout.read(4000)
            print("progress:" + str(progress), file=sys.stdout, flush=True)
            progress += 1
            if len(data) == 0:
                break
            if rec.AcceptWaveform(data):
                results.append(rec.Result())
            results.append(rec.FinalResult())

        for i, res in enumerate(results):
            jres = json.loads(res)
            if 'result' not in jres:
                continue
            words = jres['result']
            for j in range(0, len(words), WORDS_PER_LINE):
                line = words[j: j + WORDS_PER_LINE]
                s = srt.Subtitle(index=len(subs),
                    content=" ".join([ln['word'] for ln in line]),
                   start=datetime.timedelta(seconds=line[0]['start']),
                   end=datetime.timedelta(seconds=line[-1]['end']))
                subs.append(s)
        return subs

    subtitle = srt.compose(transcribe())
    #print (subtitle, flush=True)
    with open(args.output, 'w', encoding='utf8') as f:
        f.writelines(subtitle)
    f.close()

if __name__ == "__main__":
    sys.exit(main())
