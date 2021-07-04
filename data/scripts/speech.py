#!/usr/bin/env python3

#pip3 install vosk
#pip3 install srt

from vosk import Model, KaldiRecognizer, SetLogLevel
import sys
import os
import wave
import subprocess
import srt
import json
import datetime

SetLogLevel(-1)

os.chdir(sys.argv[1])

if not os.path.exists(sys.argv[2]):
    print ("Please download the model from https://alphacephei.com/vosk/models and unpack as ", sys.argv[2]," in the current folder.")
    exit (1)

sample_rate=16000
model = Model(sys.argv[2])
rec = KaldiRecognizer(model, sample_rate)
rec.SetWords(True)

process = subprocess.Popen(['ffmpeg', '-loglevel', 'quiet', '-i',
                            sys.argv[3],
                            '-ar', str(sample_rate) , '-ac', '1', '-f', 's16le', '-'],
                            stdout=subprocess.PIPE)
WORDS_PER_LINE = 7

def transcribe():
    results = []
    subs = []
    progress = 0
    while True:
       data = process.stdout.read(4000)
       print("progress:" + str(progress), file = sys.stdout, flush=True)
       progress += 1
       if len(data) == 0:
           break
       if rec.AcceptWaveform(data):
           results.append(rec.Result())
    results.append(rec.FinalResult())

    for i, res in enumerate(results):
       jres = json.loads(res)
       if not 'result' in jres:
           continue
       words = jres['result']
       for j in range(0, len(words), WORDS_PER_LINE):
           line = words[j : j + WORDS_PER_LINE] 
           s = srt.Subtitle(index=len(subs), 
                   content=" ".join([l['word'] for l in line]),
                   start=datetime.timedelta(seconds=line[0]['start']), 
                   end=datetime.timedelta(seconds=line[-1]['end']))
           subs.append(s)
    return subs

subtitle = srt.compose(transcribe())
#print (subtitle)
with open(sys.argv[4], 'w',encoding='utf8') as f:
    f.writelines(subtitle)
f.close()
