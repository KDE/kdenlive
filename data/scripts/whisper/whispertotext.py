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
from whisper.utils import (
    exact_div,
    format_timestamp,
    get_end,
    get_writer,
    make_safe,
    optional_float,
    optional_int,
    str2bool,
)

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
        return abspath(join(dirname(__file__), '../../../MacOS/ffmpeg'))
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

    # whisper.load_model checks the model's SHA on each run, so directly load the model
    #model = whisper.load_model(model, device)

    default = os.path.join(os.path.expanduser("~"), ".cache")
    download_root = os.path.join(os.getenv("XDG_CACHE_HOME", default), "whisper")
    url = whisper._MODELS[model]
    checkpoint_file = os.path.join(download_root, os.path.basename(url))
    alignment_heads = whisper._ALIGNMENT_HEADS[model]
    with (
        open(checkpoint_file, "rb")
    ) as fp:
        checkpoint = torch.load(fp, map_location=device)
    del checkpoint_file
    dims = whisper.ModelDimensions(**checkpoint["dims"])
    model = whisper.Whisper(dims)
    model.load_state_dict(checkpoint["model_state_dict"])

    if alignment_heads is not None:
        model.set_alignment_heads(alignment_heads)

    loadedModel = model.to(device)

    transcribe_kwargs = {
        "task": task,
        "verbose": False,
        'patience': None,
        'length_penalty': None,
        'suppress_tokens': '-1',
        'condition_on_previous_text':True,
        'word_timestamps':True,
        'highlight_words': None,
        'max_line_count': None,
        'max_line_width': None,
        'max_words_per_line': None,
        'language': None,
        'fp16': None
    }
    if sys.platform == 'darwin':
        # Set FFmpeg path for whisper
        from os.path import abspath, dirname, join
        os.environ["PATH"] += os.pathsep + abspath(join(dirname(__file__), '../../../MacOS/'))

    output_dir = None
    output_format = None
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

    if "output_format" in transcribe_kwargs:
        output_format = transcribe_kwargs.pop("output_format")
    if "output_dir" in transcribe_kwargs:
        output_dir = transcribe_kwargs.pop("output_dir")
        if output_dir != None:
            writer = get_writer(output_format, output_dir)

    word_options = [
        "highlight_words",
        "max_line_count",
        "max_line_width",
        "max_words_per_line",
    ]
    print(f"READY TO OURPUT: {output_format} , {output_dir}", file=sys.stdout,flush=True)
    writer_args = {arg: transcribe_kwargs.pop(arg) for arg in word_options}
    if writer_args["max_line_count"] != None:
       writer_args["max_line_count"] = int(writer_args["max_line_count"])
    if writer_args["max_line_width"] != None:
       writer_args["max_line_width"] = int(writer_args["max_line_width"])

    result = loadedModel.transcribe(source, **transcribe_kwargs)
    if output_dir != None:
        writer(result, source, **writer_args)

    return result


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
