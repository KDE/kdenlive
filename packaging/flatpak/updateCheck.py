# SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartlab.uber.space>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL


import urllib.request
import json
import re

uptodate = []
noinfo = []
commitinfo = []
gitlaburls = ["gitlab.com","invent.kde.org","code.videolan.org"]

def gitlab_release(module, url, current, gitlab):
  x = re.search("(?<=https://"+gitlab+"/)(.*)", url).group().split("/")
  namespace = x[0]
  name = x[1]
  if ".git" in name:
    name = name[:-4]
  contents = json.loads(urllib.request.urlopen("https://" + gitlab + "/api/v4/projects/" + namespace + "%2F" + name + "/releases").read())
  if contents:
    ver =  re.search("([0-9]{1,}.[0-9]{1,}(?:.[0-9]{1,}){0,})", contents[0]["tag_name"])
    if current.group() == ver.group():
      uptodate.append(module)
    else:
      print("\nModule " + module)
      print(">>>>>>>>>>>>>>>\n>>> Updates available (from " + current.group() + " to " + ver.group() +")\n>>>>>>>>>>>>>>>")
  else:
    noinfo.append(name)

def gitlab_release(module, url, current, gitlab):
  x = re.search("(?<=https://"+gitlab+"/)(.*)", url).group().split("/")
  namespace = x[0]
  name = x[1]
  if ".git" in name:
    name = name[:-4]
  contents = json.loads(urllib.request.urlopen("https://" + gitlab + "/api/v4/projects/" + namespace + "%2F" + name + "/releases").read())
  if contents:
    ver =  re.search("([0-9]{1,}.[0-9]{1,}(?:.[0-9]{1,}){0,})", contents[0]["tag_name"])
    if current.group() == ver.group():
      uptodate.append(module)
    else:
      print("\nModule " + module)
      print(">>>>>>>>>>>>>>>\n>>> Updates available (from " + current.group() + " to " + ver.group() +")\n>>> " + url + "\n>>>>>>>>>>>>>>>")
  else:
    noinfo.append(name)

def github_release(module, url, current):
  x = re.search("(?<=https://github.com/)(.*)", url).group().split("/")
  owner = x[0]
  name = x[1]
  if ".git" in name:
    name = name[:-4]
  contents = json.loads(urllib.request.urlopen("https://api.github.com/repos/" + owner + "/" + name + "/releases").read())
  if contents:
    ver =  re.search("([0-9]{1,}.[0-9]{1,}(?:.[0-9]{1,}){0,})", contents[0]["tag_name"])
    if current is not None:
      if current.group() == ver.group():
        uptodate.append(module)
      else:
        print("\nModule " + module)
        print(">>>>>>>>>>>>>>>\n>>> Updates available (from " + current.group() + " to " + ver.group() +")\n>>> " + url + "\n>>>>>>>>>>>>>>>")
    else:
        print("\nModule " + module)
        print("> Last version " + ver.group())
  else:
    noinfo.append(name)

with open('org.kde.kdenlive-nightly.json', 'r') as mainfile:
    data=mainfile.read()

doc = json.loads(data)

print("Update check for: " + str(doc['app-id']))

for module in doc['modules']:
    if type(module) is not str:
        name = module['name']
        for source in module['sources']:
            srctype = source['type']
            if srctype == "git":
                if 'branch' in source and 'commit' not in source and 'tag' not in source:
                    uptodate.append(name + " (follows branch " + source['branch'] + ")")
                else:
                    commitinfo.append(name)
            if srctype == "archive":
                url = source['url']
                ver = re.search("([0-9]{1,}.[0-9]{1,}(?:.[0-9]{1,}){0,})", url)
                if "github" in url:
                    github_release(name, url, ver)
                else:
                    gitlab = None
                    for gu in gitlaburls:
                        if gu in url:
                            gitlab = gu
                    if gitlab is not None:
                        gitlab_release(name, url, ver, gitlab)
                    else:
                        noinfo.append(name)
if len(noinfo) > 0:
  print("=== NO INFO ===\n" + "\n".join(noinfo))
if len(commitinfo) > 0:
  print("===== BASED ON COMMIT =====\n" + "\n".join(commitinfo) + "\n")

if len(uptodate) > 0:
  print("=== UP-TO-DATE ===\n" + "\n".join(uptodate) + "\n")

