"""Publish completed prepared files atomically; never changes the renderer or settings."""
from pathlib import Path
import argparse, hashlib, json, os, re, stat, struct, tempfile, time

p=argparse.ArgumentParser();p.add_argument('--fixture',action='store_true');p.add_argument('--once',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1];work=root/'build/preview23/atmosphere-r44'
source=work/('installer-fixture-package/prepared-materials-final' if a.fixture else 'prepared-materials-final')
client=work/'cache-delivery-fixture-client' if a.fixture else Path('F:/AO_SInstall/Anarchy Online')
destination=client/'randy-vk/cache/materials'
ledger_path=work/('CACHE-DELIVERY-FIXTURE.json' if a.fixture else 'CACHE-DELIVERY.json')
complete=work/'PREPARATION-COMPLETE.json'

def safe(path):
    for parent in (path,*path.parents):
        try: info=parent.lstat()
        except FileNotFoundError:continue
        if getattr(info,'st_file_attributes',0)&0x400 or stat.S_ISLNK(info.st_mode):
            raise RuntimeError('Cache delivery will not follow a redirected path: '+str(parent))

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        while data:=f.read(1<<20):h.update(data)
    return h.hexdigest()

safe(source);safe(destination);destination.mkdir(parents=True,exist_ok=True)
ledger=json.loads(ledger_path.read_text()) if ledger_path.exists() else dict(result='running',files={})
ledger['result']='running';began=time.monotonic();saw_complete=False

def save(status):
    ledger.update(result=status,count=len(ledger['files']),bytes=sum(r['bytes'] for r in ledger['files'].values()),fixture=a.fixture)
    temporary=ledger_path.with_suffix('.writing');temporary.write_text(json.dumps(ledger,indent=2)+'\n');os.replace(temporary,ledger_path)

try:
    while True:
        for f in sorted(source.glob('*.rvkm')):
            if not re.fullmatch('[0-9a-f]{16}\\.rvkm',f.name):raise RuntimeError('Invalid prepared filename')
            target=destination/f.name
            if f.name in ledger['files']:
                if not target.exists() or target.stat().st_size!=ledger['files'][f.name]['bytes']:
                    raise RuntimeError('A delivered entry changed; it was preserved: '+f.name)
                continue
            safe(f);safe(target)
            with f.open('rb') as src:
                header=src.read(416)
                if len(header)!=416:raise RuntimeError('Incomplete prepared header')
                magic,version,key,stored,decoded=struct.unpack_from('<IIQQQ',header)
                size=f.stat().st_size
                if magic!=0x4d4b5652 or version!=4 or stored+416!=size or not 0<decoded<=64<<20 or stored>decoded:
                    raise RuntimeError('Unsupported or incomplete prepared entry: '+f.name)
                src.seek(0);temp=None
                try:
                    with tempfile.NamedTemporaryFile(dir=destination,prefix='.r44-',suffix='.tmp',delete=False) as out:
                        temp=Path(out.name);digest=hashlib.sha256()
                        while data:=src.read(1<<20):out.write(data);digest.update(data)
                    expected=digest.hexdigest()
                    if temp.stat().st_size!=size:raise RuntimeError('Prepared copy was truncated')
                    if target.exists():
                        if sha(target)!=expected:raise RuntimeError('Existing different entry was preserved: '+f.name)
                    else:
                        try:os.rename(temp,target) # Windows fails if another file appeared; never overwrites it.
                        except FileExistsError:
                            if sha(target)!=expected:raise RuntimeError('Existing different entry was preserved: '+f.name)
                    ledger['files'][f.name]=dict(bytes=size,sha256=expected)
                finally:
                    if temp and temp.exists():temp.unlink()
            if len(ledger['files'])%250==0:
                save('running');print('Delivered',len(ledger['files']),'prepared sets;',ledger['bytes']//1048576,'MiB',flush=True)
        finished=complete.exists()
        if finished and not saw_complete and not a.once:
            saw_complete=True;continue # Rescan once after the producers have stopped.
        if a.once or finished:
            if finished and json.loads(complete.read_text())['result']!='passed':raise RuntimeError('Texture preparation did not complete successfully')
            save('passed');print('Cache delivery complete:',ledger['count'],'sets;',ledger['bytes']//1048576,'MiB',flush=True);break
        if time.monotonic()-began>3600:raise RuntimeError('Preparation has not completed within the delivery window')
        save('running');time.sleep(5)
except BaseException:
    save('incomplete');raise
