#!/usr/bin/env python

import sys

if sys.version_info < (2,7,5) or sys.version_info >= (3,0,0) : 
  sys.stderr.write('Python version: ' + str(sys.version_info) + '\n')
  sys.stderr.write('ERROR: Python2 >= 2.7.5 required. Please set $UPCXX_PYTHON to an appropriate python install.\n')
  exit(1)

from nobs.tool_main import main

main()
