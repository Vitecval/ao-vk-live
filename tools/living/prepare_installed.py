"""Prepare targeted Living resources INSIDE the selected AO installation only.

Uses V5/V5.1 converters and authored routes. No original archive is modified,
uploaded, downloaded or scanned for unrelated models. Old developer caches are
never inputs. Publication of the generated directory is prohibited.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import struct
import sys
import uuid
import numpy as np
from PIL import Image
from ao_living_assets import Database, parse_static
from prepare_local import build, original_image
from prepare_static import build_static
import cache_v3 as cache
from city_wardrobes import FAMILIES, grade
from survey_world import placements


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def no_links(path):
    path = Path(os.path.abspath(path))
    for parent in (path, *path.parents):
        if parent.exists() and getattr(parent.lstat(), 'st_file_attributes', 0) & 0x400:
            raise ValueError('AO cache paths may not traverse links or junctions')
    return path


def prune_generations(root, keep):
    """Remove only flat, fully hash-matched generations owned by this preparer."""
    for directory in root.glob('generation-*'):
        if directory.name in keep:
            continue
        try:
            no_links(directory)
            provenance=json.loads((directory/'provenance.json').read_text())
            files=provenance['files']
            if not provenance.get('private_local_only') or set(p.name for p in directory.iterdir()) != set(files)|{'cache.receipt','provenance.json'}:
                continue
            if any(Path(name).name!=name or sha(no_links(directory/name))!=expected for name,expected in files.items()):
                continue
            for path in directory.iterdir():
                no_links(path)
                if not path.is_file():raise ValueError('Unexpected nested local cache directory')
            for path in list(directory.iterdir()):path.unlink()
            directory.rmdir()
        except (OSError,ValueError,KeyError):
            # Preserve anything changed or not recognizable as our generated data.
            continue


def fan(db, client, out, routes):
    recipe = routes['machinery']
    entries = placements(client/'cd_image/data/statels/566.pf')
    origin = recipe['rules'][0]['origin']
    placed = [p for p in entries if p['id'] == recipe['original_mesh'] and np.linalg.norm(np.array(p['position'])-origin) < .01]
    if len(placed) != 1:
        raise ValueError('Reviewed fan instance unavailable')
    model = parse_static(db.get(1010001, recipe['original_mesh']))
    parts = [None]*966
    for index, part in enumerate(model['parts']):
        pixels = np.full((1, 1), 0xff8c8c82, dtype=np.uint32)
        if part['texture']:
            im = original_image(db, part['texture'][1], part['texture'][0])[0]
            im.thumbnail((128,128))
            pixels = cache.argb(np.asarray(im))
        parts[946+index] = dict(part, matrix=placed[0]['matrix'], pixels=pixels, number=946+index)
    for number, expected in zip(recipe['original_parts'], recipe['rules']):
        part = parts[number]
        if cache.geometry_hash(part['vertices'], part['indices'], part['matrix']) != expected['hash']:
            raise ValueError('Fan geometry fingerprint differs from reviewed original')
    chimneys = [p for p in entries if p['id'] == recipe['chimney_mesh']]
    chimney = min(chimneys, key=lambda p: np.linalg.norm(np.array(p['position'])-recipe['outlet']))
    cap = parse_static(db.get(1010001, recipe['chimney_mesh']))['parts'][0]
    parts[926] = dict(cap, matrix=chimney['matrix'])
    # Retain the exact original pivot/animation/material implementation.
    cache.machinery(parts, out)


def lenses(out):
    for name, colour in [('port', (174,28,18)), ('starboard', (20,152,72)), ('service', (36,112,160))]:
        vertices = [[.055*math.cos(t), .055*math.sin(t), 0, 0, 0, 1,
                     .5+.45*math.cos(t), .5+.45*math.sin(t)] for t in np.arange(16)*math.tau/16]
        vertices.append([0,0,.012,0,0,1,.5,.5])
        indices = [v for i in range(16) for v in (16,i,(i+1)%16)]
        mesh = cache.basic(vertices, indices, 16)
        mesh['maps'][0][:] = 0xffd0d8da
        mesh['maps'][3][:] = 0xff000000 | colour[0]<<16 | colour[1]<<8 | colour[2]
        cache.save(out/('lens-'+name+'.lrm'), mesh)


def wardrobes(out, receipts, config, themes):
    cache.variants(out, out, receipts)
    for theme in themes:
        profile = config['themes'][theme]
        for family, (original, styles, ids) in enumerate(FAMILIES):
            if not any(x['name']==original for x in receipts):
                continue
            receipt = next(x for x in receipts if x['name'] == original)
            tiles = [i for i,x in enumerate(receipt['textures']) if x['id'] in ids]
            if len(tiles) != len(ids):
                raise ValueError('Original clothing material family incomplete')
            reference = cache.mesh(out/('civilian-'+styles[0]+'.lrm'))
            dim = reference['header'][5]
            side = dim//4
            atlas = reference['maps'][0].copy()
            first = len(receipt['textures'])
            for variant in range(3):
                for index, tile in enumerate(tiles):
                    target = tile if variant == 0 else first+(variant-1)*len(tiles)+index
                    if target >= 16:
                        raise ValueError('Clothing atlas overflow')
                    sy,sx = divmod(tile,4)
                    dy,dx = divmod(target,4)
                    block = reference['maps'][0][sy*side:(sy+1)*side,sx*side:(sx+1)*side]
                    atlas[dy*side:(dy+1)*side,dx*side:(dx+1)*side] = grade(block,profile['colours'][family*3+variant])
            for style in styles:
                mesh = cache.mesh(out/('civilian-'+style+'.lrm'))
                mesh['maps'][0] = atlas
                cache.save(out/f'cw-{theme}-{style}.lrm', mesh)


def prepare(client, metadata, playfields, review=False):
    client, metadata = no_links(client), Path(metadata).resolve(strict=True)
    if not client.is_dir() or not (client/'AnarchyOnline.exe').is_file():
        raise ValueError('Select the installed Anarchy Online directory')
    database=no_links(client/'cd_image/data/db')
    for path in database.glob('ResourceDatabase.*'):no_links(path)
    root = no_links(client/'randy-vk/living-local')
    if shutil.disk_usage(client).free < 2 << 30:
        raise ValueError('Allow at least 2 GiB free for the full local city wardrobe cache and safe staging')
    root.mkdir(parents=True, exist_ok=True)
    cache.local_cache_root(root)
    # One preparation process per installation; an interrupted lock is explicit.
    lock = root/'preparing.lock'
    try:
        with lock.open('x') as stream:stream.write(str(os.getpid()))
    except FileExistsError:
        raise ValueError('Local preparation is running or was interrupted. Close the preparation helper before removing living-local/preparing.lock and retrying.')
    stage = root/('generation-'+uuid.uuid4().hex)
    stage.mkdir()
    activated=False
    try:
        db = Database(client/'cd_image/data/db')
        expected = json.loads((metadata/'routes-v4.json').read_text())
        if db.index_sha256 != expected['source_index']:
            raise ValueError('Unsupported resource database index; no routes or assets approved')
        catalog = json.loads((metadata/'original-catalog.json').read_text())
        config = json.loads((metadata/'CITY-WARDROBES.json').read_text())
        source_fields=json.loads((metadata/'playfield-sources-preview22.json').read_text())
        source_hashes={}
        for pf in playfields:
            path=no_links(client/f'cd_image/data/statels/{pf}.pf')
            if source_fields.get(str(pf))!=sha(path):
                raise ValueError(f'Unsupported placement data for playfield {pf}; no old routes approved')
            source_hashes[f'source-{pf}.pf']=source_fields[str(pf)]
        sources = [metadata/'city-wardrobes-v5'/f'{pf}.lrs' for pf in playfields]
        for source in sources:
            if not source.is_file():
                raise ValueError(f'Unsupported playfield: {source.stem}')
        receipts, errors = [], {}
        for item in catalog['assets']:
            try:
                print('Preparing original animated asset: '+item['name'], flush=True)
                receipts.append(build(db, client, item, stage))
            except Exception as error:
                errors[item['name']] = str(error)
        for item in catalog['statics']:
            try:
                print('Preparing original service vehicle: '+item['name'], flush=True)
                receipts.append(build_static(db, client, item, stage))
            except Exception as error:
                errors[item['name']] = str(error)
        themes = {config['cities'][str(pf)]['theme'] for pf in playfields if str(pf) in config['cities']}
        wardrobes(stage, receipts, config, themes)
        if (stage/'ao-hoverdrone.lrm').is_file():
            drone = cache.mesh(stage/'ao-hoverdrone.lrm')
            orm = cache.rgba(drone['maps'][2]); orm[:,:,2]=12; orm[:,:,1]=np.maximum(orm[:,:,1],205)
            orm[:135,:300,2]=220; orm[:135,:300,1]=157; drone['maps'][2]=cache.argb(orm)
            emission=cache.rgba(drone['maps'][3]); emission[210:420,395:402,:3]=0
            for y in (210,263,316,369): emission[y:y+35,395:402,:3]=(24,123,109)
            drone['maps'][3]=cache.argb(emission); cache.save(stage/'ao-hoverdrone.lrm',drone)
        try:
            print('Preparing verified fan components', flush=True)
            fan(db, client, stage, expected)
        except Exception as error:
            errors['ao-verified-fan-rotors'] = str(error)
        lenses(stage)
        spec=importlib.util.spec_from_file_location('installed_ground',metadata/'ground-data.py')
        ground=importlib.util.module_from_spec(spec); spec.loader.exec_module(ground)
        class Adapter:
            def execute(self, query, args):
                self.data=db.get(1000009,args[0])[12:]; return self
            def fetchone(self): return (self.data,)
        required=set()
        for source in sources:
            pf=int(source.stem)
            print(f'Preparing local terrain support and preserved routes: {pf}',flush=True)
            lines=source.read_text().splitlines()
            mc=int(lines[0].split()[4]); required.update(lines[1:1+mc])
            heights=ground.load(Adapter(),pf)
            H=heights['heights']; name=f'ground-{pf}.lrg'
            (stage/name).write_bytes(struct.pack('<4If',0x3152474c,pf,H.shape[1],H.shape[0],heights['cell'])+H.astype('<f4').tobytes())
            required.update((source.name,name))
            (stage/source.name).write_text('\n'.join(lines)+'\n')
        (stage/'review.ini').write_text('[LivingRubiKa]\nReviewUnverifiedRoutes='+str(int(review))+'\n')
        required.add('review.ini')
        # Remove temporary local atlases and intermediate variants before activation.
        for path in stage.iterdir():
            if path.name not in required:
                path.unlink()
        files={p.name:sha(p) for p in stage.iterdir() if p.is_file()}
        receipt_files=files|source_hashes
        (stage/'cache.receipt').write_text('LRLOCAL1 '+db.index_sha256+' '+str(len(receipt_files))+'\n'+''.join(f'{n} {h}\n' for n,h in sorted(receipt_files.items())))
        provenance={'schema':1,'source_index':db.index_sha256,'files':files,'assets':receipts,
                    'errors':errors,'playfields':playfields,'source_playfield_hashes':source_hashes,'private_local_only':True,
                    'permissions':'User-authorized local preparation; no claim of Funcom permission.',
                    'live_verified':False}
        (stage/'provenance.json').write_text(json.dumps(provenance,indent=2))
        if (root/'active.txt').is_file():
            (root/'previous.txt').write_text((root/'active.txt').read_text())
        (root/'active.next').write_text(stage.name+'\n')
        os.replace(root/'active.next',root/'active.txt')
        activated=True
        keep={stage.name}
        if (root/'previous.txt').is_file():keep.add((root/'previous.txt').read_text().strip())
        prune_generations(root,keep)
        print('Local cache activated; original archives unchanged. '+str(len(errors))+' unsupported asset families.',flush=True)
        return provenance
    finally:
        if not activated and stage.exists() and stage.resolve().parent==root.resolve():
            # This fresh generation is flat and belongs exclusively to this
            # invocation. Never recurse into links, unknown folders or archives.
            paths=list(stage.iterdir())
            if all(no_links(path).is_file() for path in paths):
                for path in paths:path.unlink()
                stage.rmdir()
        lock.unlink(missing_ok=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client',required=True,type=Path)
    parser.add_argument('--metadata',required=True,type=Path)
    parser.add_argument('--playfield',type=int,action='append')
    parser.add_argument('--review-routes',action='store_true',help='Explicit isolated static-route review; not live certification')
    args=parser.parse_args()
    fields=args.playfield or [int(p.stem) for p in (args.metadata/'city-wardrobes-v5').glob('*.lrs')]
    try:prepare(args.client,args.metadata,fields,args.review_routes)
    except Exception as error:
        print('Local Living preparation failed: '+str(error),file=sys.stderr,flush=True)
        raise SystemExit(1)


if __name__=='__main__':
    main()
