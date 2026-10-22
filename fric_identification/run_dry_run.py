"""Build/run the SAME C++ controller on a PC; never opens a serial or CAN device."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
from convert_friction import convert


def main():
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', help='g++, clang++, or zig executable')
    parser.add_argument('--output', type=Path, default=root.parent/'.build'/'friction_verification')
    parser.add_argument('--plot', action='store_true')
    args = parser.parse_args()
    compiler = args.compiler or shutil.which('g++') or shutil.which('clang++')
    local_zig = root.parent/'.build/friction_tools/ziglang/zig.exe'
    if not compiler and local_zig.exists():
        compiler = str(local_zig)
    if not compiler:
        parser.error('Install a C++17 compiler or pass --compiler')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    executable = out/('core_test.exe' if os.name == 'nt' else 'core_test')
    command = [compiler]
    if Path(compiler).stem == 'zig':
        command += ['c++']
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=os.environ.get(
        'ZIG_GLOBAL_CACHE_DIR', str(root.parent/'.build'/'zig_cache')))
    command += ['-std=c++17', '-O2', '-DFRICTION_NO_MOTOR=1',
                '-I'+str(root), str(root/'tests/core_test.cpp'), '-o', str(executable)]
    subprocess.run(command, check=True, env=env)
    binary = out/'simulation.BIN'
    subprocess.run([str(executable), str(binary)], check=True)
    subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(root/'tests'), '-v'], check=True)
    print(convert(binary))
    if args.plot:
        from plot_friction import plot
        print(plot(binary.with_suffix('.csv')))


if __name__ == '__main__':
    main()
