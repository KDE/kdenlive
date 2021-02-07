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
    print ("Please download the model from https://alphacephei.com/vosk/models and unpack as ",sys.argv[2]," in the current folder.")
    exit (1)

sample_rate=16000
model = Model(sys.argv[2])
rec = KaldiRecognizer(model, sample_rate)

process = subprocess.Popen(['ffmpeg', '-loglevel', 'quiet', '-i',
                            sys.argv[3],
                            '-ar', str(sample_rate) , '-ac', '1', '-f', 's16le', '-'],
                            stdout=subprocess.PIPE)
WORDS_PER_LINE = 7

def transcribe():
    results = []
    subs = []
    while True:
       data = process.stdout.read(4000)
       if len(data) == 0:
           break
       if rec.AcceptWaveform(data):
           sentence = rec.Result()
           print (sentence)

transcribe()
#with open(sys.argv[3], 'w') as f:
#    f.writelines(subtitle)
#f.close()
