#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import os
import re
import subprocess
import sys
import requests
from transformers import AutoProcessor, SeamlessM4Tv2Model

def main(**kwargs):
    kwargs_def = {
        'task':'',
    }

    assert all(k in kwargs_def for k in kwargs), f"Invalid kwargs: {kwargs.keys()}"
    kwargs = { **kwargs_def, **kwargs }
    task = kwargs['task']
    if task == "download":
        processor = AutoProcessor.from_pretrained("facebook/seamless-m4t-v2-large")
        model = SeamlessM4Tv2Model.from_pretrained("facebook/seamless-m4t-v2-large")
    else:
        print ("Usage:", flush=True)
        print ("task=download : download seamless m4t v2 model and weight", flush=True)

    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main(**dict(arg.split('=') for arg in sys.argv[1:])))
