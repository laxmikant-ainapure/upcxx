#!/usr/bin/env python

import sys

assert sys.version_info >= (2,6)

if sys.version_info[0] != 2:
  import os
  os.execvp('python2', sys.argv)

from nobs.tool_main import main

try:
  main()
except KeyboardInterrupt:
  exit(1)
