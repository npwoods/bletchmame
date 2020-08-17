#!/usr/bin/python

import os
import re
import sys
import subprocess

# invoke git to get the tags
git_tags = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE)

# invoke git to get the revision
git_revision = subprocess.run(["git", "rev-parse", "HEAD"], stdout=subprocess.PIPE)

# invoke date
date_time = subprocess.run(["date", "-Ins"], stdout=subprocess.PIPE)

# match the regular expression
m = re.match('v(\d+).(\d+)(-\d+)\n?', git_tags.stdout.decode('utf-8'))

# extract major and minor build numbers
major = m.group(1)
minor = m.group(2)
subminor = '0'

# extract build number
if m.group(3):
	build = m.group(3).replace('-', '')
else:
	build = '0'

# and output it
print("static const char buildVersion[] = \"" + major + "." + minor + "." + subminor + "." + build + "\";")
print("static const char buildRevision[] = \"" + git_revision.stdout.decode('utf-8').rstrip() + "\";")
print("static const char buildDateTime[] = \"" + date_time.stdout.decode('utf-8').rstrip() + "\";")
