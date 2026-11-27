"""Actual isolated DLL settings check; no game launch, Vulkan device, or retail writes."""
from pathlib import Path
import argparse,configparser,hashlib,json,shutil,subprocess,time

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--dll',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--catalog',type=Path,default=Path('apps/horizon/Assets/settings.json'));args=ap.parse_args()
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    report=dict(result='running',scope='Actual settings exports in an isolated DLL copy; no GPU/device/game/live client',fixtureSha256=sha(args.fixture),dllSha256=sha(args.dll),catalogSha256=sha(args.catalog),runs=[],retailClientsChanged=False,settingsMigration='Missing keys use enabled defaults; existing explicit material values are not forced to1')
    try:
        catalog=json.loads(args.catalog.read_text(encoding='utf-8-sig'))
        for key in ('RVK_TerrainHD','RVK_BuildingHD'):
            assert next(s['defaultValue'] for s in catalog['settings'] if s['key']==key)==1,key
            assert all(key not in p['values'] for p in catalog['presets']),key+' should remain a personal choice'
        cases=[('fresh','',1,1),('missing-building','[Renderer]\nRVK_TerrainHD=0\nPersonalUnknown=17\n[Display]\nMode=Windowed\n',0,1),('explicit-choices','[Renderer]\nRVK_TerrainHD=0.35\nRVK_BuildingHD=0\nPersonalUnknown=17\n[Display]\nMode=Windowed\n',.35,0)]
        for name,initial,terrain,building in cases:
            directory=out/name;directory.mkdir();exe=directory/'preview23_settings_check.exe';dll=directory/'ao-vk.dll'
            shutil.copy2(args.fixture,exe);shutil.copy2(args.dll,dll)
            ini=directory/'randy-vk.ini'
            if initial:ini.write_text(initial,encoding='ascii')
            for phase in ('first-load','restart'):
                started=time.monotonic();p=subprocess.run([str(exe),'--check-material-defaults',str(terrain),str(building)],cwd=directory,capture_output=True,text=True,timeout=60)
                (directory/(phase+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
                report['runs'].append(dict(case=name,phase=phase,exitCode=p.returncode,seconds=round(time.monotonic()-started,3),expectedTerrain=terrain,expectedBuilding=building,iniSha256=sha(ini) if ini.exists() else None))
                assert p.returncode==0,f'{name}/{phase}: {p.stdout} {p.stderr}'
                config=configparser.ConfigParser(interpolation=None);config.read(ini,encoding='utf-8-sig')
                assert abs(config.getfloat('Renderer','RVK_TerrainHD')-terrain)<.001
                assert abs(config.getfloat('Renderer','RVK_BuildingHD')-building)<.001
                if initial:
                    assert config.get('Renderer','PersonalUnknown')=='17'
                    assert config.get('Display','Mode')=='Windowed'
            assert sha(dll)==report['dllSha256'] and sha(exe)==report['fixtureSha256']
        report['result']='passed'
    except Exception as e:report['result']='failed';report['error']=str(e);raise
    finally:(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
