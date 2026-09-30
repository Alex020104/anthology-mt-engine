"""Build the LAN branch without modifying or installing the single-player engine."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--configuration', choices=['DX11', 'DX11-AVX', 'all'], default='all')
    parser.add_argument('--output', type=Path, default=ROOT.parent / 'dist/bin')
    parser.add_argument('--msbuild', type=Path)
    args = parser.parse_args()
    msbuild = args.msbuild
    if not msbuild:
        vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        found = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.Component.MSBuild', '-find', 'MSBuild/**/Bin/MSBuild.exe'], text=True).splitlines()
        if not found:
            raise SystemExit('MSBuild not found')
        msbuild = Path(found[0])
    args.output.mkdir(parents=True, exist_ok=True)
    logs = ROOT.parent / 'build-logs'
    logs.mkdir(exist_ok=True)
    for configuration in (['DX11', 'DX11-AVX'] if args.configuration == 'all' else [args.configuration]):
        name = 'AnomalyDX11AVX_LAN' if configuration.endswith('AVX') else 'AnomalyDX11_LAN'
        label = time.strftime('%Y%m%d-%H%M%S') + '-' + configuration
        env = os.environ.copy()
        env['_LINK_'] = '/DEBUG:NONE /MAP:"' + str(logs / (label + '.map')) + '"'
        command = [str(msbuild), str(ROOT / 'src/engine-vs2022.sln'), '/m:2', '/t:xrEngine',
                   '/p:Configuration=' + configuration, '/p:Platform=x64', '/p:BuildInParallel=true',
                   '/p:CL_MPCount=3', '/p:ForceImportBeforeCppTargets=' + str(ROOT / 'tools/build_lan.props'),
                   '/v:minimal', '/nologo']
        print('Building', configuration, 'log:', logs / (label + '.log'), flush=True)
        started = time.time()
        with (logs / (label + '.log')).open('wb') as log:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                                    creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
        record = dict(configuration=configuration, exit_code=result.returncode, seconds=time.time()-started,
                      source=str(ROOT), commit=subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(), command=command)
        if result.returncode == 0:
            source = ROOT / '_build/_game/bin_dbg' / (name + '.exe')
            target = args.output / source.name
            temporary = target.with_suffix('.tmp')
            shutil.copy2(source, temporary)
            temporary.replace(target)
            record.update(exe=str(target), sha256=hashlib.sha256(target.read_bytes()).hexdigest())
        (logs / (label + '.json')).write_text(json.dumps(record, indent=2), encoding='utf-8')
        print(json.dumps(record), flush=True)
        if result.returncode:
            return result.returncode
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
