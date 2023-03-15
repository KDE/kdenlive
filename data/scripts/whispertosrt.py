#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import whisper
import srt
import sys
import os
import wave
import subprocess
import codecs
import datetime

# Call this script with the following arguments
# 1. source av file
# 2. model name (tiny, base, small, medium, large)
# 3. output .srt file
# 4. Device (cpu, cuda)
# 5. translate or transcribe
# 6. Language
# 7. in point (optional)
# 8. out point
# 9. tmp file name to extract a clip's part

if sys.platform == 'darwin':
    from os.path import abspath, dirname, join
    path = abspath(join(dirname(__file__), '../../MacOS/ffmpeg'))
else:
    path = 'ffmpeg'

sample_rate=16000
source=sys.argv[1]
# zone rendering
if len(sys.argv) > 8 and (float(sys.argv[7])>0 or float(sys.argv[8])>0):
    process = subprocess.run([path, '-loglevel', 'quiet', '-y', '-i',
                            sys.argv[1], '-ss', sys.argv[7], '-t', sys.argv[8],
                            '-vn', '-ar', str(sample_rate) , '-ac', '1', '-f', 'wav', sys.argv[9]],
                            stdout=subprocess.PIPE)
    source=sys.argv[9]


model = whisper.load_model(sys.argv[2], device=sys.argv[4])
if len(sys.argv[6]) > 1 :
    result = model.transcribe(source, task=sys.argv[5], language=sys.argv[6], verbose=False)
else :
    result = model.transcribe(source, task=sys.argv[5], verbose=False)

subs = []
for i in range(len(result["segments"])):
    start_time = result["segments"][i]["start"]
    end_time = result["segments"][i]["end"]
    duration = end_time - start_time
    timestamp = f"{start_time:.3f} - {end_time:.3f}"
    text = result["segments"][i]["text"]

    sub = srt.Subtitle(index=len(subs), content=text, start=datetime.timedelta(seconds=start_time), end=datetime.timedelta(seconds=end_time))
    subs.append(sub)

subtitle = srt.compose(subs)
#print (subtitle)
with open(sys.argv[3], 'w',encoding='utf8') as f:
    f.writelines(subtitle)
f.close()


