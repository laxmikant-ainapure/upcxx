#!/usr/bin/env python

import sys

if sys.version_info < (2,7,5): # send to stderr to ensure install visibility
  sys.stdout.write('ERROR: Python2 >= 2.7.5 required.\n')
  sys.stderr.write('ERROR: Python2 >= 2.7.5 required.\n')
  exit(1)

if sys.version_info[0] != 2:
  import os
  os.execv('/usr/bin/env', ['/usr/bin/env','python2'] + sys.argv)

from nobs.tool_main import main

main()
