#!/usr/bin/env python3
"""Prepare source for review without Git mutations, optional artwork or binaries."""
import argparse,hashlib,json,shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DIRECTORIES=['cmake','docs','ghidra-scripts','interface','proxy','rvk','tests','third_party','tools','release','licenses','material-packs','.github']
ROOT_FILES=['.gitignore','.gitattributes','README.md','CHANGELOG.md','CMakeLists.txt','CMakePresets.json','LICENSE','THIRD-PARTY-NOTICES.md','RELEASE-NOTES.md']
EXCLUDED_PARTS={'.git','.codex','.agents','.aws','__pycache__','private'}
EXCLUDED_SUFFIXES={'.dll','.exe','.lib','.obj','.pdb','.pyc','.zip','.7z','.log','.dmp','.cache','.tmp','.bak','.bmp'}

def eligible(path):
 rel=path.relative_to(ROOT)
 if EXCLUDED_PARTS.intersection(rel.parts) or path.suffix.lower() in EXCLUDED_SUFFIXES:return False
 if rel.parts[0]=='material-packs' and len(rel.parts)>2 and rel.parts[2] in {'maps','source-art'}:return False
 return path.name not in {'randy-vk.ini','ao-vk-live.install.json','last-pick.txt'}

def digest(path):
 with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--export',type=Path);a=p.parse_args()
 paths={ROOT/name for name in ROOT_FILES if (ROOT/name).is_file()}
 for directory in DIRECTORIES:
  paths.update(f for f in (ROOT/directory).rglob('*') if f.is_file() and eligible(f))
 files=[]
 for path in sorted(paths):
  size=path.stat().st_size
  if size>=50*1024*1024:raise RuntimeError(f'Unexpected large source input: {path.relative_to(ROOT)} ({size} bytes)')
  files.append({'path':path.relative_to(ROOT).as_posix(),'bytes':size,'sha256':digest(path)})
 result={'format':1,'files':files,'file_count':len(files),'bytes':sum(x['bytes'] for x in files),'large_review_items':[x for x in files if x['bytes']>=1024*1024],
  'excluded':'Builds/dist, optional material-packs/*/maps and source-art, binary/debug/temp files, private references. No workspace files removed.',
  'essential_embedded_assets':[x for x in files if x['path'].startswith('rvk/assets/')]}
 if a.export:
  if a.export.exists():raise RuntimeError('Choose a new source snapshot directory; existing snapshots are preserved.')
  for x in files:
   destination=a.export/x['path'];destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/x['path'],destination)
 a.manifest.parent.mkdir(parents=True,exist_ok=True);a.manifest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({'source_files':result['file_count'],'source_bytes':result['bytes'],'large_review_items':[{k:x[k] for k in ['path','bytes']} for x in result['large_review_items']],'optional_artwork_exported':False},indent=2))

if __name__=='__main__':main()
