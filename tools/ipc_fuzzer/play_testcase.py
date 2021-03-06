#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around chrome.

Replaces all the child processes (renderer, GPU, plugins and utility) with the
IPC fuzzer. The fuzzer will then play back a specified testcase.

Depends on ipc_fuzzer being available on the same directory as chrome.
"""

import argparse
import os
import platform
import subprocess
import sys

def main():
  desc = 'Wrapper to run chrome with child processes replaced by IPC fuzzers'
  parser = argparse.ArgumentParser(description=desc)
  parser.add_argument('--out-dir', dest='out_dir', default='out',
                      help='ouput directory under src/ directory')
  parser.add_argument('--build-type', dest='build_type', default='Release',
                      help='Debug vs. Release build')
  parser.add_argument('--chrome-flags', dest='chrome_flags',
                      help='comma-separated additional flags to pass to chrome')
  parser.add_argument('testcase');
  parsed = parser.parse_args();

  chrome_binary = 'chrome'
  fuzzer_binary = 'ipc_fuzzer_replay'

  script_path = os.path.realpath(__file__)
  ipc_fuzzer_dir = os.path.dirname(script_path)
  src_dir = os.path.abspath(os.path.join(ipc_fuzzer_dir, os.pardir, os.pardir))
  out_dir =  os.path.join(src_dir, parsed.out_dir);
  build_dir = os.path.join(out_dir, parsed.build_type)

  chrome_path = os.path.join(build_dir, chrome_binary)
  if not os.path.exists(chrome_path):
    print 'chrome executable not found at ', chrome_path
    return 1

  fuzzer_path = os.path.join(build_dir, fuzzer_binary)
  if not os.path.exists(fuzzer_path):
    print 'fuzzer executable not found at ', fuzzer_path
    print ('ensure GYP_DEFINES="enable_ipc_fuzzer=1" and build target ' +
           fuzzer_binary + '.')
    return 1

  prefixes = {
    '--renderer-cmd-prefix',
    '--gpu-launcher',
    '--plugin-launcher',
    '--ppapi-plugin-launcher',
    '--utility-cmd-prefix',
  }

  args = [
    chrome_path,
    '--ipc-fuzzer-testcase=' + sys.argv[-1],
    '--no-sandbox',
    '--disable-kill-after-bad-ipc',
  ]

  launchers = {}
  for prefix in prefixes:
    launchers[prefix] = fuzzer_path

  if parsed.chrome_flags:
    for arg in parsed.chrome_flags.split(','):
      if arg.find('=') != -1:
        switch, value = arg.split('=', 1)
        if switch in prefixes:
          launchers[switch] = value + ' ' + launchers[switch]
          continue
      args.append(arg)

  for switch, value in launchers.items():
    args.append(switch + '=' + value)

  print args

  return subprocess.call(args)


if __name__ == "__main__":
  sys.exit(main())
