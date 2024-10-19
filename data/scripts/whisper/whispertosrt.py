#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import datetime
import sys
import os

import whispertotext

# Call this script with the following arguments
# 1. source av file
# 2. model name (tiny, base, small, medium, large)
# 3. output .srt file

# Optional keywords:

# device = cpu, cuda
# task = translate or transcribe
# language
# max_line_width = max number of characters in a subtitle
# shorten_method = greedy, halving (shortening method to cut subtitles when max_line_width is set
# zone_in (in point)
# zone_out (out point)
# tmpfile (tmp file name to extract a clip's part)
# fp16 = False to disable fp16

def main(source, model, **kwargs):
    kwargs_def = {
        'device':'cpu',
        'task':'transcribe',
        'language':'',
        'max_line_width': None,
        'max_line_count': None,
        'shorten_method': 'greedy',
        'zone_in':-1,
        'zone_out':-1,
        'tmpfile':'',
        'fp16': True,
        'seamless_source':'',
        'seamless_target':''
    }
    assert all(k in kwargs_def for k in kwargs), f"Invalid kwargs: {kwargs.keys()}"
    kwargs = { **kwargs_def, **kwargs }

    device = kwargs['device']
    language = kwargs['language']
    task = kwargs['task']
    shorten_method = kwargs['shorten_method']
    zone_in = int(kwargs['zone_in'])
    zone_out = int(kwargs['zone_out'])
    tmpfile = kwargs['tmpfile']
    fp16 = kwargs['fp16'] != 'False'

    outFolder = os.path.dirname(source)
    if tmpfile:
        whispertotext.extract_zone(source, zone_in, zone_out, tmpfile)
        source = tmpfile
    args = ''
    if language:
        args = f"language={language} "
    args += f"fp16={fp16} "
    args += f"output_format=srt "
    args += f"output_dir={outFolder} "
    if kwargs['max_line_width'] != None:
        args += f"max_line_width={kwargs['max_line_width']} "

    if kwargs['max_line_count'] != None:
        args += f"max_line_count={kwargs['max_line_count']} "

    result = whispertotext.run_whisper(source, model, device, task, args)

    #if kwargs['seamless_source']:
    #    print(f"0%| initialize", file=sys.stdout,flush=True)
    #    from transformers import AutoProcessor, SeamlessM4Tv2Model
    #    import torch
    #    processor = AutoProcessor.from_pretrained("facebook/seamless-m4t-v2-large")
    #    seamlessmodel = SeamlessM4Tv2Model.from_pretrained("facebook/seamless-m4t-v2-large")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], # source AV file
         sys.argv[2], # model name
         **dict(arg.split('=') for arg in sys.argv[3:]))) # kwargs
