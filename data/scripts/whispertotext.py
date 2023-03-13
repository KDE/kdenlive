#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import whisper
import sys
import os
import wave
import subprocess
import codecs
import datetime

# Call this script with the following arguments
# 1. source av file
# 2. model name (tiny, base, small, medium, large)
# 3. Device (cpu, cuda)
# 4. translate or transcribe
# 5. Language
# 6. in point (optional)
# 7. out point
# 8. tmp file name to extract a clip's part

if sys.platform == 'darwin':
    from os.path import abspath, dirname, join
    path = abspath(join(dirname(__file__), '../../MacOS/ffmpeg'))
else:
    path = 'ffmpeg'

sample_rate=16000
source=sys.argv[1]
# zone rendering
if len(sys.argv) > 7 and (float(sys.argv[6])>0 or float(sys.argv[7])>0):
    process = subprocess.run([path, '-loglevel', 'quiet', '-i',
                            sys.argv[1], '-ss', sys.argv[6], '-t', sys.argv[7],
                            '-vn', '-ar', str(sample_rate) , '-ac', '1', '-f', 'wav', sys.argv[8]],
                            stdout=subprocess.PIPE)
    source=sys.argv[8]

#else:
#    process = subprocess.run([path, '-loglevel', 'quiet', '-y', '-i',
#                            sys.argv[1],
#                            '-ar', str(sample_rate) , '-ac', '1', '-f', 's16le', '/tmp/out.wav'])


model = whisper.load_model(sys.argv[2], device=sys.argv[3])
if len(sys.argv[5]) > 1 :
    result = model.transcribe(source, task=sys.argv[4], language=sys.argv[5], verbose=False)
else :
    result = model.transcribe(source, task=sys.argv[4], verbose=False)

for i in range(len(result["segments"])):
    start_time = result["segments"][i]["start"]
    end_time = result["segments"][i]["end"]
    duration = end_time - start_time
    timestamp = f"{start_time:.3f} - {end_time:.3f}"
    text = result["segments"][i]["text"]
    res = '[' + str(start_time) + '>' + str(end_time) + ']' + text + '\n'
    sys.stdout.buffer.write(res.encode('utf-8'))

#sys.stdout.buffer.write(result["json"].encode('utf-8'))
sys.stdout.flush()


