"""Windows stdlib-only integration check: python tests/material_download_tests.py.

Uses a loopback HTTP server and the real 32-bit PowerShell streaming downloader.
The production pinned GitHub URL is checked before a test-only transport override.
No public release or game installation is contacted or changed.
"""
from pathlib import Path
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import argparse, hashlib, json, os, subprocess, threading, zipfile, io, uuid, importlib.util

ROOT=Path(__file__).resolve().parents[1]
VERSION='0.6.0-preview.21'
BASE=f'https://github.com/Vitecval/ao-vk-live/releases/download/v{VERSION}/'
def sha(b):return hashlib.sha256(b).hexdigest()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=ROOT/'build/material-download-tests');args=parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    out=output/('run-'+uuid.uuid4().hex[:10]);out.mkdir()
    payload={};parts=[];assets={}
    for n in [1,2]:
        data=bytes(range(256))*8192;relative=f'randy-vk/packs/ao-cyber-world/1010004_{120+n}_d.png'
        b=io.BytesIO()
        with zipfile.ZipFile(b,'w',zipfile.ZIP_STORED) as z:z.writestr(relative,data)
        name=f'ao-vk-live-{VERSION}-windows-x86-full-materials-{n}.zip';payload[name]=b.getvalue();assets[relative]=sha(data)
        parts.append(dict(name=name,sha256=sha(payload[name]),bytes=len(payload[name]),unpackedBytes=len(data),files=[relative]))
    requests=[];modes={}
    class Handler(BaseHTTPRequestHandler):
        protocol_version='HTTP/1.1'
        def log_message(self,*a):pass
        def do_GET(self):
            case,name=self.path.strip('/').split('/',1);offset=int(self.headers.get('Range','bytes=0-').split('=')[1].split('-')[0]);requests.append(dict(case=case,name=name,offset=offset))
            data=payload[name]
            if modes.get(case)=='corrupt':data=data[:-1]+bytes([data[-1]^255])
            self.send_response(206 if offset else 200);self.send_header('Content-Length',str(len(data)-offset))
            if offset:self.send_header('Content-Range',f'bytes {offset}-{len(data)-1}/{len(data)}')
            self.end_headers()
            body=data[offset:]
            if modes.get(case)=='interrupt':body=body[:100000]
            try:self.wfile.write(body);self.wfile.flush()
            except (ConnectionError,OSError):pass
            self.close_connection=True
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    host=Path(os.environ['WINDIR'])/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    env={k:v for k,v in os.environ.items() if k.lower()!='psmodulepath'}
    env['PSModulePath']=str(host.parent/'Modules')+';'+os.environ.get('PSModulePath','')
    results=[]
    spec=importlib.util.spec_from_file_location('material_archives',ROOT/'tools/material-archives.py')
    packager=importlib.util.module_from_spec(spec);spec.loader.exec_module(packager)
    fixture=out/'packaging'/'ao-vk-live-0.6.0-preview.21-windows-x86-full';fixture.mkdir(parents=True)
    fixture_assets={}
    for name in assets:
        path=fixture/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(bytes(range(256))*8192);fixture_assets[name]=sha(path.read_bytes())
    nested='randy-vk/packs/ao-cyber-world/manifest.json';(fixture/nested).write_text('{}');fixture_assets[nested]=sha(b'{}')
    addon={}
    for name in ['ao-vk.dll','ddraw.dll','Install.cmd']:
        (fixture/name).write_bytes(b'fixture');addon[name]=sha(b'fixture')
    (fixture/'manifest.json').write_text(json.dumps(dict(version=VERSION,assets=fixture_assets,addon={k:v for k,v in addon.items() if k.endswith('.dll')},payload=dict(addon))))
    archives=packager.build_full_setup(fixture,VERSION,part_limit=3*1024*1024)
    assert len(archives)==3
    with zipfile.ZipFile(archives[0]) as z:assert fixture.name+'/'+nested in z.namelist(),'Required nested catalogue omitted from Setup'
    results.append(dict(case='complete-setup-and-multi-archive-packaging',result='passed'))
    def run(case,offline=False,expect=True,reuse=False,wrong=False,disk=False):
        d=out/case;d.mkdir(exist_ok=True);client=d/'dummy client';client.mkdir(exist_ok=True)
        for name in ['randy-vk.ini','ao-vk.dll','ao-vk-live.install.json']:
            p=client/name
            if not p.exists():p.write_text('preserved owner '+name)
        before={p.name:sha(p.read_bytes()) for p in client.iterdir()}
        manifest=dict(version=VERSION,assets=assets,materialParts=parts,materialDownload=dict(repository='Vitecval/ao-vk-live',tag='v'+VERSION,baseUrl=BASE),installBytes=4*1024*1024)
        if wrong:manifest['materialDownload']['tag']='v0.6.0-preview.20'
        (d/'manifest.json').write_text(json.dumps(manifest))
        if offline:
            for name,b in payload.items():(d/name).write_bytes(b)
        local=f'http://127.0.0.1:{server.server_port}/{case}/'
        script=d/'test.ps1'
        script.write_text(f'''$ErrorActionPreference='Stop'
. '{ROOT / 'tools/material-parts.ps1'}'
function Open-AoMaterialResponse([string]$Url,[long]$Offset) {{
    if (!$Url.StartsWith('{BASE}',[StringComparison]::Ordinal)) {{throw 'Unpinned release request'}}
    {'throw "Offline fixture must not contact the network"' if offline else ''}
    $urlLocal=$Url.Replace('{BASE}','{local}')
    $request=[Net.HttpWebRequest]::Create($urlLocal);$request.Timeout=3000;$request.ReadWriteTimeout=3000
    if($Offset -gt 0){{$request.AddRange($Offset)}}
    return $request.GetResponse()
}}
{'function Get-AoMaterialFreeBytes([string]$Path) { return [long]0 }' if disk else ''}
$manifest=Get-Content -LiteralPath '{d / 'manifest.json'}' -Raw|ConvertFrom-Json
$script:events=0
Expand-AoMaterialParts '{d}' $manifest '{client}' {{param($message,$percent) $script:events++}}
if($script:events -lt 1){{throw 'Missing setup progress'}}
''',encoding='utf-8')
        start=len(requests)
        p=subprocess.run([str(host),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(script)],capture_output=True,text=True,timeout=90,env=env)
        (d/('repeat.log' if reuse else 'run.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
        assert (p.returncode==0)==expect,(case,p.stdout,p.stderr)
        assert before=={p.name:sha(p.read_bytes()) for p in client.iterdir()},'Download failure changed existing installation'
        if expect:
            assert all(sha((d/name).read_bytes())==h for name,h in assets.items())
        else:assert not any((d/name).exists() for name in assets),'Failed preparation exposed a partial material set'
        if offline or wrong or disk or reuse:assert len(requests)==start,'Unexpected network request'
        results.append(dict(case=case,result='passed',expected_success=expect,network_requests=requests[start:]))
        return d,p
    try:
        d,_=run('automatic')
        for name in assets:(d/name).unlink()
        run('automatic',reuse=True)
        run('offline',offline=True)
        modes['interrupted']='interrupt';d,p=run('interrupted',expect=False)
        assert list((d/'.ao-vk-downloads').glob('*.partial')),'Interrupted bytes were not retained'
        modes['interrupted']='normal';start=len(requests);run('interrupted')
        assert any(x['offset']>0 for x in requests[start:]),'Retry failed to resume'
        modes['corrupt']='corrupt';d,p=run('corrupt',expect=False);assert 'SHA256' in p.stdout+p.stderr
        run('wrong-release',wrong=True,expect=False)
        run('no-space',disk=True,expect=False)
        # The existing installer suite injects a commit failure and checks exact
        # restoration of DLLs, all optional pack files, receipt and preferences.
        p=subprocess.run([str(host),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tests/install-live.tests.ps1')],capture_output=True,text=True,timeout=180,env=env)
        (out/'rollback.log').write_text(p.stdout+p.stderr,encoding='utf-8');assert p.returncode==0,p.stdout+p.stderr
        results.append(dict(case='transactional-install-rollback',result='passed'))
        receipt=dict(result='passed',host=str(host),cases=results,transport='loopback HTTP with the real built-in streaming/range code; production release pin checked before test-only transport override',public_github_contacted=False,game_changed=False)
        (out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
        (output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
        print('PASS: automatic multi-pack download, verified reuse, interrupted resume, corrupt bytes, offline install, wrong-release refusal, disk-space refusal and transactional rollback.')
    finally:server.shutdown();server.server_close()
if __name__=='__main__':main()
