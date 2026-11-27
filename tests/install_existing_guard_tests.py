"""Run the actual existing-only installer on synthetic private clients; no GPU/network."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, uuid

ROOT=Path(__file__).resolve().parents[1]
def sha(p):
    with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve()
    if not str(output).lower().startswith('f:\\ao-vk-d2\\'):
        raise ValueError('Use an authorized physical F: output folder')
    output.mkdir(parents=True,exist_ok=True)
    attempt=output/('attempt-'+uuid.uuid4().hex);attempt.mkdir()
    sources=[ROOT/'tools/install-live.ps1',ROOT/'tools/setup-safety.ps1',ROOT/'tests/install_existing_guard.tests.ps1',Path(__file__)]
    pins={str(p.relative_to(ROOT)):sha(p) for p in sources}
    report=dict(result='failed',sourceSha256=pins,hosts=[],networkContacted=False,gpuUsed=False,retailClientsChanged=False,gameLaunched=False)
    try:
        hosts=[('Core',Path(shutil.which('pwsh') or '')),
               ('Windows32',Path(os.environ['WINDIR'])/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe')]
        for name,host in hosts:
            if not host.is_file():raise RuntimeError('Required host missing: '+name)
            state=attempt/name;state.mkdir();result=state/'RESULTS.json'
            process=subprocess.run([str(host),'-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tests/install_existing_guard.tests.ps1'),'-FixtureRoot',str(state/'fixtures'),'-ResultsPath',str(result)],capture_output=True,text=True,timeout=180)
            log=state/'run.log';log.write_text(process.stdout+process.stderr,encoding='utf-8')
            entry=dict(host=name,executable=str(host),executableSha256=sha(host),exitCode=process.returncode,log=str(log),logSha256=sha(log),results=str(result))
            if result.exists():entry.update(resultsSha256=sha(result),report=json.loads(result.read_text(encoding='utf-8-sig')))
            report['hosts'].append(entry)
            if process.returncode or entry.get('report',{}).get('result')!='passed':
                raise RuntimeError('Existing-only installer guard failed: '+name+'; inspect '+str(log))
        if any(sha(ROOT/name)!=pin for name,pin in pins.items()):raise RuntimeError('A tested source changed during validation')
        report['result']='passed'
    except Exception as error:
        report['error']=str(error)
    (output/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
    return 0 if report['result']=='passed' else 1

if __name__=='__main__':raise SystemExit(main())
