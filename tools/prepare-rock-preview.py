"""Convert the three verified, untransformed glTF scan fixtures to renderer input.

Preserves authored positions/normals/UVs, splits only at the D3D7 16-bit index
limit, and recentres the origin. It does not simplify or deform the mesh.
"""
from pathlib import Path
import json, struct, hashlib, math

root=Path(__file__).resolve().parents[1]/'build/preview24/rocks-r48/scans'
for asset in ('rock_face_01','rock_face_02','boulder_01'):
    folder=root/asset;g=json.loads((folder/(asset+'.gltf')).read_text())
    assert len(g['nodes'])==1 and set(g['nodes'][0])<= {'name','mesh'}
    assert len(g['meshes'])==1 and len(g['meshes'][0]['primitives'])==1
    buffers=[(folder/Path(b['uri']).name).read_bytes() for b in g['buffers']]
    def accessor(index):
        a=g['accessors'][index];view=g['bufferViews'][a['bufferView']]
        assert 'sparse' not in a and not a.get('normalized',False)
        code={5126:'f',5123:'H',5125:'I'}[a['componentType']]
        count={'VEC3':3,'VEC2':2,'SCALAR':1}[a['type']];fmt='<'+code*count
        stride=view.get('byteStride',struct.calcsize(fmt));at=view.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,buffers[view['buffer']],at+i*stride) for i in range(a['count'])]
    primitive=g['meshes'][0]['primitives'][0];assert primitive.get('mode',4)==4
    p=accessor(primitive['attributes']['POSITION']);n=accessor(primitive['attributes']['NORMAL']);uv=accessor(primitive['attributes']['TEXCOORD_0'])
    indices=[x[0] for x in accessor(primitive['indices'])];assert len(indices)%3==0 and max(indices)<len(p)
    low=[min(v[k] for v in p) for k in range(3)];high=[max(v[k] for v in p) for k in range(3)]
    centre=[(low[0]+high[0])/2,low[1],(low[2]+high[2])/2]
    parts=[];mapping={};vertices=[];out=[]
    def flush():
        if out:parts.append((vertices.copy(),out.copy()))
        mapping.clear();vertices.clear();out.clear()
    for start in range(0,len(indices),3):
        triangle=indices[start:start+3]
        if len(mapping)+sum(source not in mapping for source in set(triangle))>60000:flush()
        for source in triangle:
            if source not in mapping:
                mapping[source]=len(vertices)
                vertex=tuple(p[source][k]-centre[k] for k in range(3))+n[source]+(0xFFFFFFFF,)+uv[source]
                assert all(math.isfinite(v) for v in vertex)
                vertices.append(vertex)
            out.append(mapping[source])
    flush();path=folder/'mesh.rvkmesh'
    with path.open('wb') as f:
        f.write(b'RVKSCN1\0');f.write(struct.pack('<I',len(parts)))
        for vertices,out in parts:
            f.write(struct.pack('<II',len(vertices),len(out)))
            for vertex in vertices:f.write(struct.pack('<6fI2f',*vertex))
            f.write(struct.pack('<'+'H'*len(out),*out))
    result=dict(asset=asset,triangles=len(indices)//3,sourceVertices=len(p),chunks=len(parts),
        boundsMetres=[low,high],originShift=centre,geometrySimplified=False,meshSha256=hashlib.sha256(path.read_bytes()).hexdigest())
    (folder/'mesh-verified.json').write_text(json.dumps(result,indent=2)+'\n');print(result,flush=True)
