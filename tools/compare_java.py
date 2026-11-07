#!/usr/bin/env python3
"""Regenerate Java observations in a temporary directory, compare immutable fixtures and C++.
Requires a JDK and the upstream checkout at the commit recorded in THIRD_PARTY.md.
Never edits the reference checkout, the C++ source, fixtures, or shipped disk images.
"""
import argparse
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def run(cmd, cwd):
    return subprocess.run([str(x) for x in cmd], cwd=cwd, check=True, text=True,
                          stdout=subprocess.PIPE).stdout


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference', type=Path, required=True)
    p.add_argument('--tns', type=Path, required=True)
    p.add_argument('--javac', default='javac', help='Compiler command; e.g. "java -jar /tmp/ecj.jar"')
    args = p.parse_args()
    repo = Path(__file__).resolve().parent.parent
    reference = args.reference.resolve()
    binary = args.tns.resolve()
    head = run(['git', '-C', reference, 'rev-parse', 'HEAD'], repo).strip()
    if head != 'b50b0bf8e7c1dd1a5b31ce913b59d1f89733e131':
        raise RuntimeError('Reference commit differs from the recorded upstream commit: ' + head)
    with tempfile.TemporaryDirectory(prefix='tns-oracle-') as temp:
        root = Path(temp)
        src, classes = root / 'src', root / 'classes'
        src.mkdir(); classes.mkdir()
        for package in ('machine', 'z80core', 'utils'):
            shutil.copytree(reference / 'src' / package, src / package)
        # AWT lock-state queries require a desktop; the oracle never sends host key events.
        keyboard = src / 'machine' / 'Keyboard.java'
        text = keyboard.read_text(encoding='utf-8')
        text = text.replace('KeyEvent;;', 'KeyEvent;')  # Rejects in Eclipse compiler.
        text = text.replace('Toolkit.getDefaultToolkit()', 'null')
        import re
        text = re.sub(r'toolkit.getLockingKeyState\([^)]*\)', 'false', text)
        keyboard.write_text(text, encoding='utf-8')
        (src / 'gui').mkdir()
        (src / 'gui' / 'Screen.java').write_text(
            'package gui; public class Screen { public void repaint() {} }\n')
        for name in ('BootOracle.java', 'CpuOracle.java'):
            shutil.copyfile(repo / 'tests' / 'reference' / name, src / name)
        sources = sorted(src.rglob('*.java'))
        run(shlex.split(args.javac) + ['-encoding', 'UTF-8', '-d', str(classes)] + sources, root)
        shutil.copytree(reference / 'src' / 'roms', classes / 'roms')
        shutil.copyfile(reference / 'src' / 'machine' / 'Bundle.properties',
                        classes / 'machine' / 'Bundle.properties')
        java = ['java', '-Djava.awt.headless=true', '-cp', classes]
        for mode in ('gc', 'itk'):
            observed = ''.join(x + '\n' for x in run(java + ['BootOracle', mode], root).splitlines()
                               if x.startswith('TRACE '))
            fixture = (repo / 'tests' / ('java-' + mode + '.trace')).read_text()
            if observed != fixture:
                raise RuntimeError('Java ' + mode + ' observation differs from committed fixture')
            port = run([binary, '--headless', '--frames', '100', '--trace', '--no-save', '--' + mode], root)
            if observed != port:
                raise RuntimeError('C++ ' + mode + ' differs from Java')
        cpu = run(java + ['CpuOracle'], root)
        if cpu != (repo / 'tests' / 'java-cpu.trace').read_text():
            raise RuntimeError('Java CPU observation differs from committed fixture')
        disk = root / 'demo.8sd'
        shutil.copyfile(repo / 'media' / 'tnsdemo.8sd', disk)
        demo = ''.join(x + '\n' for x in run(java + ['BootOracle', 'gc', disk], root).splitlines()
                       if x.startswith('TRACE '))
        if demo != (repo / 'tests' / 'java-demo.trace').read_text():
            raise RuntimeError('Java CP/M observation differs from committed fixture')
        tests = binary.with_name('tns_tests.exe' if binary.suffix == '.exe' else 'tns_tests')
        print(run([tests, repo / 'tests'], root), end='')
    print('PASS: live Java/C++ comparison, 3584 CPU cases and 700 frames including CP/M')


if __name__ == '__main__':
    main()
