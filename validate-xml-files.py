#!/usr/bin/python3
# Version in sysadmin/ci-utilities should be single source of truth
# SPDX-FileCopyrightText: 2023 Alexander Lohnau <alexander.lohnau@gmx.de>
# SPDX-FileCopyrightText: 2024 Johnny Jazeix <jazeix@gmail.com>
# SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import glob
import os
import subprocess
import yaml
import xml.etree.ElementTree as ET
from typing import Optional

parser = argparse.ArgumentParser(description='Check XML files in repository')
parser.add_argument('--check-all', default=False, action='store_true', help='If all files should be checked or only the changed files')
parser.add_argument('--verbose', default=False, action='store_true')
args = parser.parse_args()

if not args.check_all:
    print("Note: only validating files that changed in git, use -check-all to validate all files")

supported_extensions = (('.xml', '.kcfg', '.ui', '.qrc'))
xsd_map = {
        "data/effects/": {
            "schema": "data/kdenlive-assets.xsd",
            "excludes": ["data/effects/templates"]
        },
        # "data/transitions/": { "schema": "data/kdenlive-assets.xsd" }
    }

def get_changed_files() -> list[str]:
    result = subprocess.run(['git', 'diff', '--cached', '--name-only'], capture_output=True, text=True)
    return [file for file in result.stdout.splitlines() if file.endswith(supported_extensions)]

def get_all_files() -> list[str]:
    files = []
    for root, _, filenames in os.walk('.'):
        for filename in filenames:
            if filename.endswith(supported_extensions):
                files.append(os.path.join(root, filename))
    return files

def filter_excluded_included_xml_files(files: list[str]) -> list[str]:
    config_file = '.kde-ci.yml'
    # Check if the file exists
    if os.path.exists(config_file):
        with open(config_file, 'r') as file:
            config = yaml.safe_load(file)
    else:
        if args.verbose:
            print(f'{config_file} does not exist in current directory')
        config = {}
    # Extract excluded files, used for tests that intentionally have broken files
    excluded_files = []
    if 'Options' in config and 'xml-validate-ignore' in config['Options']:
        xml_files_to_ignore = config['Options']['xml-validate-ignore']
        for xml in xml_files_to_ignore:
            excluded_files += glob.glob(xml, recursive=True)

    # Find XML files
    filtered_files = []
    for file_path in files:
        if not any(excluded_file in file_path for excluded_file in excluded_files):
            filtered_files.append(file_path)

    # "include" overrides the "ignore" files, so if a file is both included and excluded, it will be included
    if 'Options' in config and 'xml-validate-include' in config['Options']:
        for filename in config['Options']['xml-validate-include']:
            if os.path.isfile(filename):
                filtered_files += [filename]
            else:
                print(f"warning: {filename} does not exist, please double check it and either fix the filename or update the .kde-ci.yml file to remove it")


    return filtered_files

def get_files() -> list[str]:
    if args.check_all:
        files = get_all_files()
    else:
        files = get_changed_files()
    return filter_excluded_included_xml_files(files)

def do_xmllinting(files: list[str], schema: Optional[str] = None):
    if not files:
        return

    files_option = ' '.join(files)

    for xml in files:
        if args.verbose:
            if schema:
                print(f"Validating {xml} with schema {schema}")
            else:
                print(f"Validating {xml}")

        xmllint_args = ["xmllint", "--noout"]
        if schema:
            xmllint_args += ["--noout", "--schema", schema]
        xmllint_args += [xml]
        result = subprocess.run(xmllint_args, stderr=subprocess.PIPE, stdout=subprocess.DEVNULL)

        # Fail the pipeline if command failed
        if result.returncode != 0:
            print(result.stderr.decode())
            exit(1)

        if args.verbose:
            print(result.stderr.decode())

def read_effectscategory() -> list[str]:
    tree = ET.parse('data/kdenliveeffectscategory.rc')
    root = tree.getroot()

    effect_ids = []

    for group in root:
        if group.tag != "group":
            continue
        effects = group.attrib["list"].split(",")
        effect_ids += effects

    return effect_ids

def read_asset_id(filepath: str, ignoreHidden=True) -> Optional[str]:
    tree = ET.parse(filepath)
    root = tree.getroot()

    effects = []

    if root.tag.endswith("group"):
        for child in root:
            effects += [child]
    else:
        effects = [root]

    for effect in effects:
        if not effect.tag.endswith("effect"):
            return None

        if ignoreHidden and "type" in effect.attrib and effect.attrib["type"] == "hidden":
            continue

        if "id" in effect.attrib:
            return effect.attrib["id"]

        if "tag" in effect.attrib:
            return effect.attrib["tag"]

    print(f"Note: can not load asset id (or skipped because hidden) for {filepath}")
    return None

def validate_effects(files: list[str]):
    missing = []
    if args.verbose:
        print(f"Check if effects have category {files}")

    for xml in files:
        effect_id = read_asset_id(xml)
        if effect_id and not effect_id in effect_ids:
            missing += [effect_id]

    if missing:
        missing = ", ".join(missing)
        print(f"Some effects are missing in \"kdenliveeffectscategory.rc\": {missing}")
        exit(1)

files = sorted(get_files())
effect_ids = read_effectscategory()

for key in xsd_map:
    matches = []
    others = []
    excludes = xsd_map[key].get("excludes", [])
    while files:
        xml = files.pop().lstrip("./")
        excluded = any(path in xml for path in excludes)
        if not excluded and xml.startswith(key.lstrip("./")):
            matches += [xml]
        else:
            others += [xml]

    xsd_map[key]["files"] = matches
    files = others

print(f"## Linting {len(files)} files without schema")
do_xmllinting(files)

for key in xsd_map:
    file_set = xsd_map[key]
    xmls = file_set["files"]
    schema = file_set["schema"]

    print(f"## Linting {len(xmls)} files starting with {key} with schema {schema}")
    do_xmllinting(xmls, schema)

    #if key == "./data/effects/":
        #validate_effects(xmls)
