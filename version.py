#!/usr/bin/python

import re
import sys

# match the regular expression
m = re.match('v(\d+).(\d+)(-\d+)?', sys.stdin.readline())

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
print(major + "." + minor + "." + subminor + "." + build)
