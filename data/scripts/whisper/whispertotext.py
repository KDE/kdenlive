#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import codecs
import datetime
import os
import re
import subprocess
import sys
import wave

import torch
import whisper

# Call this script with the following arguments
# 1. source av file
# 2. model name (tiny, base, small, medium, large)
# 3. Device (cpu, cuda)
# 4. translate or transcribe
# 5. Language
# 6. in point (optional)
# 7. out point
# 8. tmp file name to extract a clip's part

def avoid_fp16(device):
    """fp16 doesn't work on some GPUs, such as Nvidia GTX 16xx. See bug 467573."""
    if device == "cpu": # fp16 option doesn't matter for CPU
        return True
    device = torch.cuda.get_device_name(device)
    if re.search(r"GTX 16\d\d", device):
        sys.stderr.write("GTX 16xx series GPU detected, disabling fp16\n")
        return True

def ffmpeg_path():
    if sys.platform == 'darwin':
        from os.path import abspath, dirname, join
        return abspath(join(dirname(__file__), '../../MacOS/ffmpeg'))
    else:
        return 'ffmpeg'


def extract_zone(source, outfile, in_point, out_point):
    sample_rate = 16000
    path = ffmpeg_path()
    process = subprocess.run([path, '-loglevel', 'quiet', '-y', '-i',
                            source, '-ss', in_point, '-t', out_point,
                            '-vn', '-ar', str(sample_rate) , '-ac', '1', '-f', 'wav', outfile],
                            stdout=subprocess.PIPE)


def run_whisper(source, model, device="cpu", task="transcribe", extraparams=""):
    model = whisper.load_model(model, device)

    transcribe_kwargs = {
        "task": task,
        "verbose": False,
        'patience': None,
        'length_penalty': None,
        'suppress_tokens': '-1',
        'condition_on_previous_text':True,
        'word_timestamps':True
    }
    if sys.platform == 'darwin':
        # Set FFmpeg path for whisper
        from os.path import abspath, dirname, join
        os.environ["PATH"] += os.pathsep + abspath(join(dirname(__file__), '../../MacOS/'))

    if len(extraparams) > 1:
        extraArgs = extraparams.split()
        for x in extraArgs:
            param = x.split('=')
            if (len(param) > 1):
                transcribe_kwargs[param[0]] = param[1]

    if 'fp16' in transcribe_kwargs and transcribe_kwargs['fp16'] == 'False':
        transcribe_kwargs["fp16"] = False
    elif avoid_fp16(device):
        transcribe_kwargs["fp16"] = False

    return model.transcribe(source, **transcribe_kwargs)


def main():
    source=sys.argv[1]
    if len(sys.argv) > 8 and (float(sys.argv[6])>0 or float(sys.argv[7])>0):
        tmp_file = sys.argv[8]
        extract_zone(source, tmp_file, sys.argv[6], sys.argv[7])
        source = tmp_file

    model = sys.argv[2]
    device = sys.argv[3]
    task = sys.argv[4]
    language = sys.argv[5]
    result = run_whisper(source, model, device, task, language)

    for i in result["segments"]:
        start_time = i["start"]
        end_time = i["end"]
        duration = end_time - start_time
        timestamp = f"{start_time:.3f} - {end_time:.3f}"
        text = i["text"]
        words = i["words"]
        res = '[' + str(start_time) + '>' + str(end_time) + ']' + '\n'
        for j in words:
            res += '[' + str(j["start"]) + '>' + str(j["end"]) + ']' + j["word"] + '\n'
        sys.stdout.buffer.write(res.encode('utf-8'))
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
