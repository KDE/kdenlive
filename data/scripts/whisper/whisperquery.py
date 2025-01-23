#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import os
import sys
import requests
import whisper


def main(**kwargs):
    kwargs_def = {
        'task': '',
        'model': '',
        'url': '',
        'download_root': ''
    }
    assert all(k in kwargs_def for k in kwargs), f"Invalid kwargs: {kwargs.keys()}"
    kwargs = {**kwargs_def, **kwargs}
    task = kwargs['task']
    if task == "list":
        models = whisper.available_models()
        for m in models:
            url = whisper._MODELS[m]
            print(m + " : " + url, flush=True)
        default = os.path.join(os.path.expanduser("~"), ".cache")
        root = os.path.join(os.getenv("XDG_CACHE_HOME", default), "whisper")
        print("root_folder : " + root, flush=True)
    elif task == "size":
        model = kwargs['model']
        if model == '':
            print('Please give a model name', flush=True)
            # Abort
            sys.exit()

        url = whisper._MODELS[model]
        if url == '':
            print('Cannot find url for model name', flush=True)
            # Abort
            sys.exit()
        default = os.path.join(os.path.expanduser("~"), ".cache")
        root = os.path.join(os.getenv("XDG_CACHE_HOME", default), "whisper")
        download_target = os.path.join(root, os.path.basename(url))
        if os.path.exists(download_target) and os.path.isfile(download_target):
            # model is already downloaded
            print(model + " : 0", flush=True)
        else:
            resp = requests.get(url, stream=True)
            if resp.status_code != 200:
                # not ok, not found or maybe offline
                print(model + " : -1", flush=True)
            else:
                size = resp.headers.get("Content-length")
                print(model + " : " + str(size), flush=True)
    elif task == "download":
        url = kwargs['url']
        path = kwargs['download_root']
        if url == '' or path == '':
            print('Please give an url and a path', flush=True)
            # Abort
            sys.exit()
        whisper._download(url, path, False)
    else:
        print("Usage:", flush=True)
        print("task=list : list available models", flush=True)

    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main(**dict(arg.split('=') for arg in sys.argv[1:])))
