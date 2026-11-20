"""Host-only C++ comparison plus binary format tests. Never opens a device."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
workspace = root.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', help='g++, clang++, or zig executable')
args = parser.parse_args()
compiler = args.compiler or shutil.which('g++') or shutil.which('clang++')
if not compiler:
    local = workspace / 'build/host_tools/ziglang/zig.exe'
    if local.exists():
        compiler = str(local)
if not compiler:
    parser.error('A host C++ compiler is required; pass --compiler')
out = workspace / 'build/trans_ctrl_test3_verification'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(workspace / 'build/zig_cache'))
env['TEMP'] = env['TMP'] = str(out)
exe = out / ('core_test.exe' if os.name == 'nt' else 'core_test')
cmd = [compiler]
if Path(compiler).stem == 'zig':
    cmd += ['c++', '-nostdlib++']
cmd += ['-std=c++17', '-O2', '-I'+str(root/'tests/stubs'), '-I'+str(root),
        str(root/'tests/core_test.cpp'), '-o', str(exe)]
subprocess.run(cmd, check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(root/'tests'), '-v'], check=True, env=env)
