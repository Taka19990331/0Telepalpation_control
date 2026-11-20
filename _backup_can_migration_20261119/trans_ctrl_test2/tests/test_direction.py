"""Compile and run the production-interface direction and differential tests."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest


class DirectionTest(unittest.TestCase):
    def test_cpp(self):
        root = Path(__file__).resolve().parents[1]
        workspace = root.parents[1]
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            compiler = str(workspace / 'build/host_tools/ziglang/zig.exe')
        out = workspace / 'build/trans_ctrl_test2_verification'
        out.mkdir(parents=True, exist_ok=True)
        cmd = [compiler]
        if Path(compiler).stem == 'zig':
            cmd += ['c++', '-nostdlib++']
        exe = out / ('direction_test.exe' if os.name == 'nt' else 'direction_test')
        cmd += ['-std=c++17', '-O2', '-I'+str(root/'tests/stubs'), '-I'+str(root),
                str(root/'tests/direction_test.cpp'), '-o', str(exe)]
        env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(workspace/'build/zig_cache'))
        env['TEMP'] = env['TMP'] = str(out)
        subprocess.run(cmd, check=True, env=env)
        subprocess.run([str(exe)], check=True, env=env)
