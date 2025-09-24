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
        'device': 'cpu',
        'task': 'transcribe',
        'language': '',
        'max_line_width': None,
        'max_line_count': None,
        'shorten_method': 'greedy',
        'zone_in': -1,
        'zone_out': -1,
        'tmpfile': '',
        'fp16': True,
        'seamless_source': '',
        'seamless_target': '',
        'ffmpeg_path': ''
    }
    assert all(k in kwargs_def for k in kwargs), f"Invalid kwargs: {kwargs.keys()}"
    kwargs = {**kwargs_def, **kwargs}

    device = kwargs['device']
    language = kwargs['language']
    task = kwargs['task']
    shorten_method = kwargs['shorten_method']
    zone_in = int(kwargs['zone_in'])
    zone_out = int(kwargs['zone_out'])
    tmpfile = kwargs['tmpfile']
    fp16 = kwargs['fp16'] != 'False'
    ffmpeg_path = kwargs['ffmpeg_path']

    outFolder = os.path.dirname(source)
    if tmpfile:
        whispertotext.extract_zone(source, zone_in, zone_out, tmpfile)
        source = tmpfile
    args = ''
    if ffmpeg_path:
        args = f"ffmpeg_path={ffmpeg_path} "
    if language:
        args = f"language={language} "
    args += f"fp16={fp16} "
    if kwargs['seamless_source']:
        # we want to use seamless to translate, get full text from whisper
        args += "output_format=json "
    else:
        # let whisper do all the processing and write the srt file
        args += "output_format=srt "
        args += f"output_dir={outFolder} "
        if kwargs['max_line_width'] is not None:
            args += f"max_line_width={kwargs['max_line_width']} "
        if kwargs['max_line_count'] is not None:
            args += f"max_line_count={kwargs['max_line_count']} "

    result = whispertotext.run_whisper(source, model, device, task, args)

    if kwargs['seamless_source']:
        print("0%| initialize", file=sys.stdout, flush=True)
        from transformers import AutoProcessor, SeamlessM4Tv2Model
        import srt
        processor = AutoProcessor.from_pretrained("facebook/seamless-m4t-v2-large")
        seamlessmodel = SeamlessM4Tv2Model.from_pretrained("facebook/seamless-m4t-v2-large")
        loadedSeamlessModel = seamlessmodel.to(device)

        # Seamless
        subs = []
        subCount = len(result["segments"])
        for i in range(len(result["segments"])):
            start_time = result["segments"][i]["start"]
            end_time = result["segments"][i]["end"]
            duration = end_time - start_time
            timestamp = f"{start_time:.3f} - {end_time:.3f}"
            text = result["segments"][i]["text"]

            progress = int(100*i / subCount)
            print(f"{progress}%| translating", file=sys.stdout, flush=True)
            text_inputs = processor(text, src_lang=kwargs['seamless_source'], return_tensors="pt").to(device)
            output_tokens = loadedSeamlessModel.generate(**text_inputs, tgt_lang=kwargs['seamless_target'], generate_speech=False)
            translated_text_from_text = processor.decode(output_tokens[0].tolist()[0], skip_special_tokens=True)
            sub = srt.Subtitle(index=len(subs), content=translated_text_from_text, start=datetime.timedelta(seconds=start_time), end=datetime.timedelta(seconds=end_time))
            subs.append(sub)

        if kwargs['max_line_width'] is None:
            subtitle = srt.compose(subs)
        else:
            try:
                import srt_equalizer
            except ModuleNotFoundError:
                # srt equalizer not found, disable srt_equalizer
                subtitle = srt.compose(subs)
            else:
                # Reduce line length in the whisper result to <= maxLength chars
                equalized = []
                for sub in subs:
                    # shorten method was added recently so not always available
                    # equalized.extend(srt_equalizer.split_subtitle(sub, max_line_width, method=shorten_method))
                    equalized.extend(srt_equalizer.split_subtitle(sub, int(kwargs['max_line_width'])))
                subtitle = srt.compose(equalized)

        # Get output filename
        base = os.path.splitext(source)[0]
        outfile = base + ".srt"

        with open(outfile, 'w', encoding='utf8') as f:
            f.writelines(subtitle)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1],  # source AV file
         sys.argv[2],  # model name
         **dict(arg.split('=') for arg in sys.argv[3:]))) # kwargs
