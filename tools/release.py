#!/usr/bin/env python3
"""Build two clean interactive ISOs, prove identity, and package reviewable artifacts."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

REPO = Path(__file__).resolve().parents[1]
OUTPUT = REPO / 'build/release'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git',*args],cwd=REPO,text=True).strip()


def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    epoch = os.environ.get('SOURCE_DATE_EPOCH','1794960000')
    environment = dict(os.environ,SOURCE_DATE_EPOCH=epoch,LC_ALL='C')
    subprocess.run(['python3','tools/iso_date.py'],cwd=REPO,env=environment,check=True,capture_output=True)
    builds = []
    for suffix in ('a','b'):
        relative = Path('build') / ('release-repro-'+suffix)
        build = REPO / relative
        if build.exists(): shutil.rmtree(build)
        command = ['make','-j2',f'BUILD={relative}','BOOT_TEST=desktop','KERNEL_LOG_LEVEL=INFO','iso','check-elf']
        with (OUTPUT / ('repro-'+suffix+'.log')).open('wb') as log:
            subprocess.run(command,cwd=REPO,env=environment,stdout=log,stderr=subprocess.STDOUT,check=True)
        builds.append(build)
    for name in ('kernel.elf','agenticos.iso'):
        assert builds[0].joinpath(name).read_bytes() == builds[1].joinpath(name).read_bytes(), f'Non-reproducible {name}; inspect build/release/repro-*.log'
    # A relocated source checkout must produce the same compact boot image.
    # Full debug ELFs retain assembler/toolchain source paths for debugging.
    relocated = REPO/'build/release-moved-source'
    if relocated.exists(): shutil.rmtree(relocated)
    relocated.mkdir(parents=True)
    for name in ('kernel','userland','boot','tools','scripts','tests'):
        shutil.copytree(REPO/name,relocated/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
    for name in ('Makefile','VERSION','LICENSE'): shutil.copy2(REPO/name,relocated/name)
    shutil.copytree(REPO/'.cache/limine',relocated/'.cache/limine')
    with (OUTPUT/'repro-relocated.log').open('wb') as log:
        subprocess.run(['make','-j2','BUILD=build/portable','BOOT_TEST=desktop','KERNEL_LOG_LEVEL=INFO','iso','check-elf'],cwd=relocated,env=environment,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert (relocated/'build/portable/agenticos.iso').read_bytes() == (builds[0]/'agenticos.iso').read_bytes(), 'Relocating source changed the runtime ISO'
    version = (REPO/'VERSION').read_text().strip()
    files = {}
    for source,name in ((builds[0]/'agenticos.iso',f'agenticos-{version}.iso'),(builds[0]/'kernel.elf',f'agenticos-{version}-kernel.elf')):
        target = OUTPUT/name; shutil.copyfile(source,target); files[name]={'sha256':digest(target),'bytes':target.stat().st_size}
    source_hash = hashlib.sha256()
    paths = subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=REPO).split(b'\0')
    for encoded in sorted(set(path for path in paths if path)):
        path = REPO/os.fsdecode(encoded)
        if path.is_file(): source_hash.update(encoded+b'\0'+hashlib.sha256(path.read_bytes()).digest())
    tools = {}
    for name in ('clang-19','ld.lld-19','nasm','objcopy','qemu-system-x86_64','xorriso'):
        result = subprocess.run([name,'--version'],cwd=REPO,env=environment,capture_output=True,text=True,check=True)
        tools[name]=(result.stdout or result.stderr).splitlines()[0]
    manifest = {'project':'AgenticOS','version':version,'commit':git('rev-parse','HEAD'),
                'dirty':bool(git('status','--porcelain')),'source_tree_sha256':source_hash.hexdigest(),
                'source_date_epoch':int(epoch),'boot_mode':'desktop','target':'x86_64 QEMU q35, one CPU, RGB32 800x600',
                'reproducible':True,'relocated_source_iso_identical':True,'debug_source_root':str(REPO),'independent_builds':[str(path.relative_to(REPO)) for path in builds],
                'tools':tools,'artifacts':files,'tests':'Run make test-release for packaged ISO boots; full regression commands are documented in docs/RELEASE.md.'}
    (OUTPUT/'manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    (OUTPUT/'SHA256SUMS').write_text(''.join(data['sha256']+'  '+name+'\n' for name,data in files.items()))
    (OUTPUT/'README.txt').write_text(f'AgenticOS {version}\n\nVerify: sha256sum -c SHA256SUMS\nBoot the ISO in QEMU with 128 MiB RAM and one CPU.\nSee docs/USER_GUIDE.md, RELEASE.md and HARDWARE_TESTING.md in the source repository.\nFiles/settings are in RAM and reset on reboot. Physical installation has not been performed.\n')
    print(f'PASS: two clean builds produce identical ELF and ISO; relocated source reproduces ISO; artifacts: {OUTPUT.relative_to(REPO)}')
    for name,data in files.items(): print(data['sha256']+'  '+name)
    return 0


if __name__=='__main__':
    raise SystemExit(main())
