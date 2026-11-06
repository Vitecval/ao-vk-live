"""Synthetic integration regression for async helper exit codes and status reporting.
No original client files, models or AO installation are inputs.
"""
from pathlib import Path
import json,hashlib,shutil,subprocess,os,uuid,argparse
S=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Run the real x86 installer and packaged helper on art-free synthetic data only.')
parser.add_argument('--helper',required=True,type=Path)
parser.add_argument('--output',required=True,type=Path)
args=parser.parse_args();O=args.output.resolve()
if not O.is_relative_to(S.resolve()):raise ValueError('Fixture output must remain inside the source workspace')
O.mkdir(parents=True,exist_ok=True)
F=O/('run-'+uuid.uuid4().hex[:8]);C=F/'Anarchy Online';P=F/'package with spaces';C.mkdir(parents=True,exist_ok=True);P.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for n in ['install-live.ps1','setup-progress.ps1','material-parts.ps1','material-download.ps1']:shutil.copy2(S/'tools'/n,P/n)
shutil.copytree(args.helper.resolve(),P/'living-preparer')
M=P/'living-metadata';M.mkdir()
D=C/'cd_image/data/db';D.mkdir(parents=True);ST=C/'cd_image/data/statels';ST.mkdir()
import struct
index=bytearray(200);struct.pack_into('<I',index,12,64);struct.pack_into('<I',index,184,1024);(D/'ResourceDatabase.idx').write_bytes(index);
(D/'ResourceDatabase.dat').write_bytes(b'unchanged synthetic archive');(ST/'566.pf').write_bytes(b'original synthetic placement')
for name,data in {'original-catalog.json':{'assets':[],'statics':[]},'CITY-WARDROBES.json':{'cities':{},'themes':{}},'routes-v4.json':{'source_index':sha(D/'ResourceDatabase.idx')},'playfield-sources-preview22.json':{'566':sha(ST/'566.pf')}}.items():(M/name).write_text(json.dumps(data))
(M/'city-wardrobes-v5').mkdir();(M/'city-wardrobes-v5/566.lrs').write_text('LRSCENE5 566 1 0 1 0 synthetic-reviewed\nmissing.lrm\n0\nground-566.lrg\n')
(M/'ground-data.py').write_text("import numpy as np\ndef load(db,pf):return {'heights':np.zeros((2,2),np.float32),'cell':4.0}\n")
originals={}

for n in ('randy31.dll','DisplaySystem.dll','Gamecode.dll','GUI.dll','N3.dll','FXS.dll'):(C/n).write_text('stock '+n);originals[n]=sha(C/n)
(C/'AnarchyOnline.exe').write_text('fixture, never executed');(C/'randy-vk.ini').write_text('[Renderer]\nRVK_LivePeople=0\n')
for n in ('ao-vk.dll','ddraw.dll'):(P/n).write_text('fixture addon, never executed')
manifest=dict(product='AO-VK Live',build='fixture-living-v1',edition='lite',client=originals,addon={n:sha(P/n) for n in ('ao-vk.dll','ddraw.dll')},assets={},previousAddons=[],livingAssets=dict(provider='installation-local-v1',status='local-test-candidate',helperSha256=sha(P/'living-preparer/ao-vk-living-prepare.exe'),files={f.relative_to(P).as_posix():sha(f) for d in ('living-preparer','living-metadata') for f in (P/d).rglob('*') if f.is_file()}))
(P/'manifest.json').write_text(json.dumps(manifest));script=F/'check.ps1'
script.write_text(f'''$ErrorActionPreference='Stop'
function global:Get-Process {{param($Name,$ErrorAction)}}
$global:progressCalls=0
$argsFixture=@{{ClientDir='{C.resolve()}';PackageDir='{P.resolve()}';ProgressCallback={{param([string]$Message,[int]$Percent) $global:progressCalls++}}}}
$output=@(& '{(S/'tools/install-live.ps1').resolve()}' @argsFixture)
$result=$output|Where-Object {{$_.AoVkSetupResult}}|Select-Object -Last 1
if ($result.livingStatus -notlike 'Living assets prepared locally*') {{throw ('Positive helper status missing: '+$result.livingStatus+' '+$result.livingWarnings)}}
if ($global:progressCalls -lt 1) {{throw 'No responsive progress callback'}}
if (!(Test-Path -LiteralPath '{C.resolve()}/randy-vk/living-local/active.txt')) {{throw 'No local generation selected'}}
[IO.File]::WriteAllText('{(ST/'566.pf').resolve()}','unsupported changed placement')
$argsFixture.Upgrade=$true
$output=@(& '{(S/'tools/install-live.ps1').resolve()}' @argsFixture)
$result=$output|Where-Object {{$_.AoVkSetupResult}}|Select-Object -Last 1
if ($result.livingStatus -notlike '*Living assets were not prepared*') {{throw 'Failed optional preparation falsely reported success'}}
if (!(Test-Path -LiteralPath '{C.resolve()}/ao-vk.dll')) {{throw 'Optional failure removed normal renderer'}}
$result|ConvertTo-Json|Set-Content -LiteralPath '{(F/'status.json').resolve()}'
Write-Output 'PASS: real helper through merged x86 installer, progress pumping, optional failure status and normal renderer retention.'
''')
host=Path(os.environ['WINDIR'])/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
p=subprocess.run([str(host),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(script.resolve())],capture_output=True,text=True,timeout=120)
(F/'run.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
assert (D/'ResourceDatabase.dat').read_bytes()==b'unchanged synthetic archive'
(O/'INSTALLER-LIVING-VALIDATION.json').write_text(json.dumps(dict(result='passed',actual_frozen_helper=True,x86_installer=True,progress=True,optional_failure_reported=True,game_files_are_synthetic=True),indent=2))
print('Actual frozen helper + x86 installer integration passed.')
