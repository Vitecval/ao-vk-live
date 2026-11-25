"""Public078 companions: real32-bit PowerShell, pinned ZIPs and loopback transport.

No public server or game installation is contacted. Legacy19 metadata is read
from the immutable077 snapshot. All writes stay in a unique test directory.
"""
from pathlib import Path
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import argparse
import copy
import gzip
import hashlib
import io
import json
import os
import subprocess
import threading
import uuid
import warnings
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BASE = 'https://github.com/Vitecval/ao-vk-live/releases/download/v0.7.8'
sha = lambda value: hashlib.sha256(value).hexdigest()


def archive(entries):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w', zipfile.ZIP_STORED) as output:
        for name, content in entries:
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning) # intentional duplicate-entry fixture
                output.writestr(name, content)
    return data.getvalue()


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/horizon-0.7.8/companion-tests')
    args = parser.parse_args()
    out = args.output.resolve()/('run-'+uuid.uuid4().hex[:10]); out.mkdir(parents=True)
    script_names = ('material-download.ps1','material-parts.ps1','material-cache.ps1')
    pinned_scripts = {name:sha((ROOT/'tools'/name).read_bytes()) for name in script_names}
    license_data = (ROOT/'LICENSE-TEXTURES.md').read_bytes()
    baseline = json.loads(gzip.decompress((ROOT/'build/horizon-0.7.7/source-final/apps/horizon/Assets/full-manifest.json.gz').read_bytes()))
    assert len(baseline['materialParts']) == 19
    (out/'legacy.json').write_text(json.dumps({key: baseline[key] for key in ('version','materialSourceVersion','materialDownload','materialParts')}))
    payloads, parts, assets, asset_data = {}, [], {}, {}
    for family, pack in [('environment','ao-penumbra-078'), ('characters','ao-reworked-characters')]:
        member = f'randy-vk/packs/{pack}/1010004_123_d.png'
        content = (family.encode()+bytes(range(256)))*4096
        name = f'ao-vk-live-0.7.8-materials-{family}-001.zip'
        payloads[name] = archive([(member, content)])
        assets[member] = sha(content)
        asset_data[member] = content
        parts.append(dict(name=name, sourceVersion='0.7.8', baseUrl=BASE, sha256=sha(payloads[name]),
                          bytes=len(payloads[name]), unpackedBytes=len(content), files=[member]))
    manifest = dict(version='0.6.0-preview.24', materialSourceVersion='0.6.0-preview.24',
                    materialDownload=baseline['materialDownload'], materialParts=parts, assets=assets,
                    artworkLicense=dict(file='LICENSE-TEXTURES.md', sha256=sha(license_data)), installBytes=0)
    requests, mode = [], {}

    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'
        def log_message(self, *unused):
            pass
        def do_GET(self):
            case, name = self.path.strip('/').split('/', 1)
            offset = int(self.headers.get('Range','bytes=0-').split('=')[1].split('-')[0])
            requests.append(dict(case=case,name=name,offset=offset))
            data = payloads[name]
            if mode.get(case) == 'corrupt':
                data = data[:-1] + bytes([data[-1]^255])
            self.send_response(206 if offset else 200)
            self.send_header('Content-Length',str(len(data)-offset))
            if offset:
                self.send_header('Content-Range',f'bytes {offset}-{len(data)-1}/{len(data)}')
            self.end_headers()
            body = data[offset:]
            if mode.get(case) == 'interrupt':
                body = body[:50000]
            try:
                self.wfile.write(body); self.wfile.flush()
            except (ConnectionError,OSError):
                pass
            self.close_connection = True

    server = ThreadingHTTPServer(('127.0.0.1',0), Handler)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    script = out/'driver.ps1'
    script.write_text(r'''param([string]$Case,[string]$Action,[string]$Transport)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. '@ROOT@/tools/material-parts.ps1'
. '@ROOT@/tools/material-cache.ps1'
$manifest=Get-Content -LiteralPath (Join-Path $Case 'manifest.json') -Raw | ConvertFrom-Json
function Open-AoMaterialResponse([string]$Url,[long]$Offset) {
    if($Url -cnotmatch '^https://github\.com/Vitecval/ao-vk-live/releases/download/v0\.7\.8/ao-vk-live-0\.7\.8-materials-(environment|characters)-[0-9]{3}\.zip$'){throw 'Unpinned fixture source'}
    if($Transport -eq 'offline'){throw 'UNEXPECTED_NETWORK'}
    $local=$Transport+'/'+[IO.Path]::GetFileName($Url)
    $request=[Net.HttpWebRequest]::Create($local);$request.Timeout=3000;$request.ReadWriteTimeout=3000
    if($Offset -gt 0){$request.AddRange($Offset)}
    return $request.GetResponse()
}
if($Action -eq 'legacy') {
    $legacy=Get-Content '@OUT@/legacy.json' -Raw | ConvertFrom-Json
    if(@($legacy.materialParts).Count -ne 19){throw 'Legacy part count changed'}
    foreach($part in $legacy.materialParts){
        Assert-AoMaterialRelease $legacy $part
        if((Get-AoMaterialDownloadUrl $legacy $part) -cne ($legacy.materialDownload.baseUrl+$part.name)){throw 'Legacy URL changed'}
        foreach($member in $part.files){if(!(Test-AoMaterialMember $part $member)){throw 'Legacy member rejected'}}
    }
    Write-Output 'Legacy19 source URLs and complete member lists accepted';exit 0
}
if($Action -eq 'policy') {
    foreach($part in $manifest.materialParts){Assert-AoMaterialRelease $manifest $part;$null=Get-AoMaterialDownloadUrl $manifest $part}
    exit 0
}
if($Action -eq 'archive-corrupt') {
    if(Test-AoMaterialArchive (Join-Path $Case $manifest.materialParts[0].name) $manifest.materialParts[0]){throw 'Corrupted archive accepted'}
    exit 0
}
if($Action -eq 'reparse') {
    $outside=Join-Path $Case 'outside';New-Item -ItemType Directory -Path $outside | Out-Null
    $link=Join-Path $Case '.ao-vk-downloads'
    # Both resolved targets are inside this unique test directory. Never follow
    # or recursively delete the junction when removing the link afterward.
    $full=[IO.Path]::GetFullPath($outside)
    if(!$full.StartsWith([IO.Path]::GetFullPath($Case)+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Bad test junction target'}
    New-Item -ItemType Junction -Path $link -Value $outside | Out-Null
    $rejected=$false
    try {Expand-AoMaterialParts $Case $manifest}
    catch {if($_.Exception.Message -notmatch 'link|junction'){throw};$rejected=$true;Write-Output 'Reparse cache rejected'}
    finally {Remove-Item -LiteralPath $link -Force}
    if(!$rejected){throw 'Unsafe reparse cache was accepted'}
    exit 0
}
if($Action -eq 'bindings') {
    $cache='randy-vk/cache/materials/0123456789abcdef.rvkm'
    $manifest.assets | Add-Member -NotePropertyName $cache -NotePropertyValue ('0'*64)
    $fields=@('0123456789abcdef')+@($manifest.assets.PSObject.Properties | Where-Object {$_.Name -like '*.png'} | ForEach-Object {$_.Name})
    while($fields.Count -lt 11){$fields+=@('')}
    $catalog=Join-Path $Case 'recipes.tsv';[IO.File]::WriteAllLines($catalog,@('RVK-RECIPES-1',($fields -join "`t")))
    Write-AoMaterialBindings $catalog (Join-Path $Case 'bindings.tsv') $Case @{} $manifest $null
    exit 0
}
Expand-AoMaterialParts $Case $manifest
foreach($entry in $manifest.assets.PSObject.Properties){if((Get-AoSetupHash (Join-Path $Case $entry.Name)) -cne $entry.Value){throw 'Decoded output hash differs'}}
'''.replace('@ROOT@',str(ROOT).replace("'","''")).replace('@OUT@',str(out).replace("'","''")))
    host = Path(os.environ['WINDIR'])/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    environment = {key:value for key,value in os.environ.items() if key.lower() != 'psmodulepath'}
    checks = []

    def run(case, value=None, action='expand', expect=True, offline=True, existing=None, raw=None, reason=None):
        assert pinned_scripts == {name:sha((ROOT/'tools'/name).read_bytes()) for name in script_names}, 'Production scripts changed during test'
        directory = existing or out/case
        directory.mkdir(exist_ok=True)
        value = copy.deepcopy(manifest if value is None else value)
        (directory/'manifest.json').write_text(json.dumps(value))
        (directory/'LICENSE-TEXTURES.md').write_bytes(license_data)
        if raw is not None:
            for name,data in raw.items():
                (directory/name).write_bytes(data)
        sentinel = directory/'not-a-game.txt'
        sentinel.write_bytes(b'untouched client sentinel')
        transport = 'offline' if offline else f'http://127.0.0.1:{server.server_port}/{case}'
        process = subprocess.run([str(host),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(script),
                                  '-Case',str(directory),'-Action',action,'-Transport',transport],
                                 capture_output=True,text=True,env=environment,timeout=180)
        (directory/(case+'.log')).write_text(process.stdout+process.stderr)
        assert (process.returncode == 0) == expect, (case,process.returncode,process.stdout,process.stderr)
        if reason:
            assert reason in process.stdout+process.stderr, (case,'Wrong rejection reason',process.stdout,process.stderr)
        assert sentinel.read_bytes() == b'untouched client sentinel'
        assert pinned_scripts == {name:sha((ROOT/'tools'/name).read_bytes()) for name in script_names}, 'Production scripts changed during test'
        if not expect and action == 'expand':
            assert not any((directory/'randy-vk/packs').glob('*/*.png')), (case,'Rejected archive published material files')
        checks.append(dict(case=case,result='passed',expectedAcceptance=expect,exitCode=process.returncode))
        print(case,'passed',flush=True)
        return directory

    receipt = dict(result='failed',checks=checks,host=str(host),publicNetworkContacted=False,gameChanged=False)
    try:
        run('legacy19-policy',action='legacy')
        run('public-two-families-offline',raw=payloads)
        downloaded = run('public-download',offline=False)
        for member in assets:
            (downloaded/member).unlink()
        start = len(requests);run('public-download-reuse',offline=False,existing=downloaded)
        assert len(requests) == start, 'Verified archive reuse contacted transport'
        run('new-pack-prepared-source-bindings',action='bindings',raw=payloads,existing=downloaded)
        for key, value in [('baseUrl','https://evil.invalid/v0.7.8'),('baseUrl',BASE+'/'),
                           ('sourceVersion','0.7.9'),('baseUrl',None),('sourceVersion',None),
                           ('name','../escape.zip'),('name','ao-vk-live-0.7.8-materials-other-001.zip')]:
            bad=copy.deepcopy(manifest);bad['materialParts'][0][key]=value
            run('reject-'+key+'-'+str(len(checks)),bad,action='policy',expect=False,reason='Invalid pinned public material')
        for path in ['../escape.png','randy-vk/packs/unknown/x.png',
                     'randy-vk/packs/ao-reworked-characters/x.png','randy-vk/packs/ao-penumbra-078/manifest.json',
                     'randy-vk/cache/materials/0123456789abcdef.rvkm','randy-vk/packs/ao-penumbra-078/sub/x.png',
                     'randy-vk\\packs\\ao-penumbra-078\\x.png']:
            bad=copy.deepcopy(manifest);bad['materialParts'][0]['files']=[path];bad['assets'][path]='0'*64
            run('reject-member-'+str(len(checks)),bad,expect=False,reason='Invalid or duplicate pinned material member')
        bad=copy.deepcopy(manifest);bad['assets'].pop(parts[0]['files'][0])
        run('reject-unpinned',bad,expect=False)
        bad=copy.deepcopy(manifest);bad.pop('artworkLicense')
        run('reject-unlicensed-overlay',bad,expect=False)
        first=parts[0];member=first['files'][0]
        for name, entries in [('unlisted-entry',[(member,asset_data[member]),('../escape.png',b'bad')]),
                               ('decoded-corruption',[(member,b'bad')]),
                               ('duplicate-entry',[(member,asset_data[member]),(member,asset_data[member])])]:
            data=archive(entries);bad=copy.deepcopy(manifest);bad['materialParts']=bad['materialParts'][:1]
            bad['materialParts'][0].update(sha256=sha(data),bytes=len(data))
            run('reject-'+name,bad,expect=False,raw={first['name']:data},reason=
                'Decoded material integrity check failed' if name=='decoded-corruption' else 'Unexpected or duplicate companion archive entry')
        corrupted=payloads[first['name']][:-1]+b'X'
        run('archive-corruption',action='archive-corrupt',raw={first['name']:corrupted})
        run('reparse-cache',action='reparse')
        mode['public-stream-corrupt']='corrupt'
        run('public-stream-corrupt',offline=False,expect=False,reason='SHA256')
        mode['public-interrupted']='interrupt';interrupted=run('public-interrupted',offline=False,expect=False)
        assert list((interrupted/'.ao-vk-downloads').glob('*.partial'))
        mode['public-interrupted']='normal';start=len(requests)
        run('public-interrupted',offline=False,existing=interrupted)
        assert any(request['offset']>0 for request in requests[start:]), 'New companion failed to resume'
        receipt['result']='passed'
    except Exception as error:
        receipt['error']=str(error)
        raise
    finally:
        server.shutdown();server.server_close()
        receipt.update(requests=requests,legacyPartsSha256=sha(json.dumps(baseline['materialParts'],sort_keys=True).encode()),scriptSha256=pinned_scripts)
        (out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
        (args.output.resolve()/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Public companion checks passed:',len(checks),out)


if __name__=='__main__':
    main()
