#! /usr/bin/python3

import subprocess

SUBTREES = { 'untangle-classd': 'git@github.com:untangle/classd.git' }

for directory, repository in SUBTREES.items():
  # FIXME: handle release branch as wwell
  cmd = 'git subtree pull --prefix={} {} master'.format(directory, repository)

  print('Updating {} from {}'.format(directory, repository))
  subprocess.call(cmd, shell=True)
