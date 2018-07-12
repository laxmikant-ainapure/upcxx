#!/usr/bin/env python

"""
show.py [-i] [<report-file>...] [<name>=<val>...]
        [-rel <name>=<val>...]

Parse the given <report-file>'s (defaults to just "report.out")
and plot their dependent variables.

The "<name>=<val>" assignments are used to filter the dataset
before processing. (Consider using "app=<app-name>" if more
than one app's are in the dataset).

If "-rel" is present, then all "<name>=<val>" pairs after it
are used as the denominator to produce relative data.

If "-i" is present than instead of showing plots, an
interactive python shell will be entered which has all the
dependent variables from the report files loaded as global
variables. Each is an instance of Table (from table.py).
"""

import sys

if '-h' in sys.argv:
  print __doc__
  sys.exit()

import table as tab

plot = tab.plot
T = tab.T

reportfiles = set()
dimvals = {}

tab.xdims['opnew'] = dict(title='Allocator')

for arg in sys.argv[1:]:
  if arg.startswith('-'):
    pass
  elif '=' not in arg:
    reportfiles.add(arg)

if len(reportfiles) == 0:
  reportfiles = ['report.out']

for arg in sys.argv[1:]:
  if arg == '-rel':
    break
  elif arg.startswith('-'):
    pass
  elif '=' in arg:
    x,y = arg.split('=')
    x = x.strip()
    y = y.split(',')
    def intify(s):
      try: return int(s)
      except: return s
    y = map(lambda y: intify(y.strip()), y)
    dimvals[x] = y

tabs = {}
for report in reportfiles:
  def emit(dependent_vars, **xys):
    dependent_vars = tuple(dependent_vars)
    if all(xys[x] in dimvals[x] for x in xys if x in dimvals):
      fact = dict(xys)
      for x in dependent_vars:
        fact.pop(x, None)
      for x in dependent_vars:
        fact1 = dict(fact)
        fact1[x] = xys[x]
        tabs[x] = tabs.get(x, [])
        tabs[x].append(fact1)
  
  execfile(report, dict(emit=emit))

for name in list(tabs.keys()):
  tabs[name], = tab.tables([name], tabs[name])

if '-rel' in sys.argv[1:]:
  relative_to = {}
  for arg in sys.argv[sys.argv.index('-rel')+1:]:
    if '=' in arg:
      x,y = arg.split('=')
      x = x.strip()
      y = y.strip()
      try: y = int(y)
      except: pass
      relative_to[x] = y
  
  for name in tabs:
    tabs[name] = tabs[name] / tabs[name].split(**relative_to)[0]
    
else:
  relative_to = {}

if '-i' in sys.argv[1:]:
  for name in tabs:
    globals()[name] = tabs[name]
  import code
  code.interact(local=globals())
else:
  titles = dict(
    bw = 'Bandwidth',
    secs = 'Elapsed (s)'
  )
  for name in tabs:
    title = titles[name]
    if relative_to:
      title += " relative to %s"%tab.pretty(relative_to)
    plot(tabs[name], title=title)
