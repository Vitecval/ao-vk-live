"""Production installer fault/recovery audit; dummy bytes, no AO process or GPU."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, uuid
ROOT=Path(__file__).resolve().parents[1]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def q(p): return "'"+str(p).replace("'","''")+"'"
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    out=a.output.resolve()/('run-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
    host=Path(os.environ['WINDIR'])/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    rows=[]
    def run(tag,script,success=True,code=None):
        file=out/(tag+'.ps1');file.write_text(script,encoding='utf-8-sig')
        r=subprocess.run([str(host),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(file)],capture_output=True,text=True,timeout=45)
        (out/(tag+'.log')).write_text(r.stdout+r.stderr,encoding='utf8')
        valid = r.returncode==(code if code is not None else 0) if success else r.returncode!=0
        assert valid,(tag,r.returncode,r.stdout,r.stderr)
        return r
    def fixture(tag,installed=True):
        directory=out/tag;client=directory/'AO folder [literal]';package=directory/'setup package';loader=directory/'shared loader'
        for d in (client,package,loader):d.mkdir(parents=True)
        originals={}
        for n in ['randy31.dll','DisplaySystem.dll','Gamecode.dll','GUI.dll','N3.dll','FXS.dll']:
            p=client/n;p.write_bytes(('stock '+n).encode());originals[n]=sha(p)
        (client/'AnarchyOnline.exe').write_text('dummy never executed');(client/'randy-vk.ini').write_text('preserve settings')
        (loader/'AnarchyOnline.exe').write_text('dummy never executed')
        assets={'randy-vk/packs/ao-cyber-world/1010004_1_d.png':'texture two'}
        addon={}
        oldaddon={}
        for n in ['ao-vk.dll','ddraw.dll']:
            (package/n).write_text('new '+n);addon[n]=sha(package/n)
            if installed:(client/n).write_text('old '+n);oldaddon[n]=sha(client/n)
        if installed:(loader/'ddraw.dll').write_text('old ddraw.dll')
        for n,v in assets.items():
            (package/n).parent.mkdir(parents=True,exist_ok=True);(package/n).write_text(v)
            if installed:(client/n).parent.mkdir(parents=True,exist_ok=True);(client/n).write_text('texture one')
        manifest=dict(product='AO-VK Live',build='new',client=originals,addon=addon,previousAddons=[oldaddon] if installed else [],assets={n:sha(package/n) for n in assets})
        (package/'manifest.json').write_text(json.dumps(manifest))
        if installed:
            receipt=dict(product='AO-VK Live',build='old',clientDirectory=str(client),loaderDirectory=str(loader),addon=oldaddon,assets={n:sha(client/n) for n in assets})
            (client/'ao-vk-live.install.json').write_text(json.dumps(receipt))
        before={str(p.relative_to(directory)):p.read_bytes() for d in (client,loader) for p in d.rglob('*') if p.is_file()}
        return directory,client,package,loader,before
    prefix="$ErrorActionPreference='Stop'\nfunction global:Get-Process {param($Name,$ErrorAction)}\n"
    def invoke(c,p,l,operation='-Upgrade'):
        return f"& {q(ROOT/'tools/install-live.ps1')} -ClientDir {q(c)} -PackageDir {q(p)} -LoaderDir {q(l)} {operation} | Out-Null\n"
    # Every backup/install move boundary: exit the *test's own* PowerShell immediately.
    for mode,total in [('update',10),('install',5),('uninstall',5)]:
        for n in range(1,total+1):
            for edge in ['before','after']:
                tag=f'power-{mode}-{n}-{edge}'
                d,c,p,l,before=fixture(tag,mode!='install')
                body=prefix+f"$global:step=0\nfunction global:Move-Item {{param($LiteralPath,$Destination)\n$isCommit=($LiteralPath -like '*.installing-*' -or $Destination -like '*.upgrade-*')\nif($isCommit){{$global:step++}}\n"
                if edge=='before':body+=f"if($isCommit -and $global:step -eq {n}){{[Environment]::Exit(93)}}\n"
                body+="Microsoft.PowerShell.Management\\Move-Item -LiteralPath $LiteralPath -Destination $Destination\n"
                if edge=='after':body+=f"if($isCommit -and $global:step -eq {n}){{[Environment]::Exit(93)}}\n"
                body+='}\n'+invoke(c,p,l,{'update':'-Upgrade','install':'','uninstall':'-Uninstall'}[mode])
                run(tag,body,code=93)
                run(tag+'-recover',prefix+f". {q(ROOT/'tools/setup-safety.ps1')}\n$m=Enter-AoSetupLock {q(c)}\ntry{{Recover-AoSetupTransaction {q(c)}}}finally{{$m.ReleaseMutex();$m.Dispose()}}")
                after={str(f.relative_to(d)):f.read_bytes() for folder in (c,l) for f in folder.rglob('*') if f.is_file()}
                assert before==after,tag
                rows.append(dict(case=tag,result='passed'))
                print('PASS',tag,flush=True)
    # Cleanup interruption happens after the durable commit marker: preserve the NEW state.
    for edge in ['before','after']:
        tag='committed-cleanup-'+edge;d,c,p,l,before=fixture(tag)
        body=prefix+"function global:Remove-Item {param($LiteralPath)\nif($LiteralPath -like '*.upgrade-*'){"+("[Environment]::Exit(94)" if edge=='before' else '')+"}\nMicrosoft.PowerShell.Management\\Remove-Item -LiteralPath $LiteralPath\nif($LiteralPath -like '*.upgrade-*'){"+("[Environment]::Exit(94)" if edge=='after' else '')+"}\n}\n"+invoke(c,p,l)
        run(tag,body,code=94)
        run(tag+'-recover',prefix+f". {q(ROOT/'tools/setup-safety.ps1')}\nRecover-AoSetupTransaction {q(c)}")
        assert sha(c/'ao-vk.dll')==sha(p/'ao-vk.dll') and sha(l/'ddraw.dll')==sha(p/'ddraw.dll')
        assert json.loads((c/'ao-vk-live.install.json').read_text())['build']=='new'
        assert not list(c.rglob('*.upgrade-*')) and not list(l.rglob('*.upgrade-*'))
        rows.append(dict(case=tag,result='passed'));print('PASS',tag,flush=True)
    # A competing installer must fail before touching any protected file.
    d,c,p,l,before=fixture('concurrent')
    blocker=out/'hold-lock.ps1';signal=out/'lock-ready';blocker.write_text(prefix+f". {q(ROOT/'tools/setup-safety.ps1')}\n$m=Enter-AoSetupLock {q(c)}\n[IO.File]::WriteAllText({q(signal)},'ready')\nStart-Sleep -Seconds 45\n",encoding='utf8')
    process=subprocess.Popen([str(host),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(blocker)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    import time
    try:
        for _ in range(600):
            if signal.exists():break
            time.sleep(.05)
        assert signal.exists()
        result=run('concurrent',prefix+invoke(c,p,l),success=False)
        assert 'Another AO-VK setup' in result.stderr
        assert before=={str(f.relative_to(d)):f.read_bytes() for folder in (c,l) for f in folder.rglob('*') if f.is_file()}
    finally:process.terminate();process.wait(timeout=10)
    rows.append(dict(case='concurrent-client-lock',result='passed'))
    run('drive-root-search',prefix+f". {q(ROOT/'tools/setup-safety.ps1')}\nforeach($p in @('C:\\AO-VK-HD','C:\\','\\\\server\\share\\Setup')){{foreach($folder in @(Get-AoMaterialSearchFolders $p ($p.TrimEnd('\\')+'\\cache'))){{$null=Join-Path $folder 'materials.zip'}}}}")
    rows.append(dict(case='drive-root-and-unc-archive-search',result='passed'))
    for fault in ['bad-json','bad-addon','unsafe-client','folder-collision','locked-file','staging-full']:
        tag='reject-'+fault;d,c,p,l,before=fixture(tag)
        manifest=json.loads((p/'manifest.json').read_text())
        if fault=='bad-json':(p/'manifest.json').write_text('{broken')
        if fault=='bad-addon':manifest['addon']['ao-vk.dll']='';(p/'manifest.json').write_text(json.dumps(manifest))
        if fault=='unsafe-client':manifest['client']['../escape.dll']='0'*64;(p/'manifest.json').write_text(json.dumps(manifest))
        if fault=='folder-collision':
            (c/'ao-vk.dll').unlink();(c/'ao-vk.dll').mkdir()
            before={str(f.relative_to(d)):f.read_bytes() for folder in (c,l) for f in folder.rglob('*') if f.is_file()}
        body=prefix
        if fault=='locked-file':body+=f"$lock=[IO.File]::Open({q(c/'ao-vk.dll')},[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)\n"
        if fault=='staging-full':body+="function global:Copy-Item {param($LiteralPath,$Destination);[IO.File]::WriteAllText($Destination,'partial');throw 'simulated disk full'}\n"
        run(tag,body+invoke(c,p,l),success=False)
        assert before=={str(f.relative_to(d)):f.read_bytes() for folder in (c,l) for f in folder.rglob('*') if f.is_file()},tag
        rows.append(dict(case=tag,result='passed'));print('PASS',tag,flush=True)
    # An uninstall race must keep newly modified data and every untouched file.
    tag='uninstall-external-change';d,c,p,l,before=fixture(tag)
    body=prefix+f"$global:changed=$false;$cb={{param($message,$percent);if(!$global:changed -and $message -like 'Installing verified files:*'){{[IO.File]::WriteAllText({q(c/'ao-vk.dll')},'user changed during setup');$global:changed=$true}}}}\n"
    body+=invoke(c,p,l,'-Uninstall -ProgressCallback $cb')
    run(tag,body,success=False)
    assert (c/'ao-vk.dll').read_text()=='user changed during setup'
    for relative,data in before.items():
        if relative!=str((c/'ao-vk.dll').relative_to(d)):assert (d/relative).read_bytes()==data,(tag,relative)
    rows.append(dict(case=tag,result='passed'));print('PASS',tag,flush=True)
    # A junction in a target ancestor must not redirect writes outside the client.
    tag='linked-target';d,c,p,l,before=fixture(tag,False);external=d/'external';external.mkdir();(external/'keep.txt').write_text('preserve external')
    rejection=run(tag,prefix+f"$null=New-Item -ItemType Junction -Path {q(c/'randy-vk')} -Target {q(external)}\n"+invoke(c,p,l,''),success=False)
    assert 'link or junction' in rejection.stderr,(rejection.stdout,rejection.stderr)
    assert (external/'keep.txt').read_text()=='preserve external' and not (c/'ao-vk.dll').exists()
    rows.append(dict(case=tag,result='passed'));print('PASS',tag,flush=True)
    tag='unicode-\u0416\u5ba2\u6237';d,c,p,l,before=fixture(tag,False)
    run('unicode-install',prefix+invoke(c,p,l,''))
    run('unicode-update',prefix+invoke(c,p,l,'-Upgrade'))
    run('unicode-uninstall',prefix+invoke(c,p,l,'-Uninstall'))
    assert before=={str(f.relative_to(d)):f.read_bytes() for folder in (c,l) for f in folder.rglob('*') if f.is_file()}
    rows.append(dict(case='unicode-install-update-uninstall',result='passed'));print('PASS unicode-install-update-uninstall',flush=True)
    (a.output.resolve()/'RESULTS.json').write_text(json.dumps(dict(result='passed',cases=rows,gameClientsChanged=False),indent=2),encoding='utf8')
if __name__=='__main__':main()
