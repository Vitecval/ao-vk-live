"""Verify restored Shared+ colour code and export independent077 GPU references.

No build/GPU work. The optional export includes verbatim preserved functions,
with distinct names and deterministic initialization of newly added map fields.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT_SHA = "9551f90bd3bd25ca8e66e776ca4ee74a14b85efef1616bd5fda6af12f0a1262d"


def function(text: str, name: str) -> str:
    match = re.search(r"(?:vec4|TerrainMap) " + re.escape(name) + r"\(", text)
    if not match:
        raise AssertionError(f"missing function {name}")
    brace = text.index("{", match.start())
    depth = 0
    for at in range(brace, len(text)):
        depth += (text[at] == "{") - (text[at] == "}")
        if depth == 0:
            return text[match.start():at + 1]
    raise AssertionError(f"unclosed function {name}")


def tokens(text: str) -> str:
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.sub(r"\s+", "", text)


def main() -> None:
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--snapshot", type=Path, default=ROOT / "build/horizon-0.7.7/source-final/rvk/shaders/terrain.glsl")
    parser.add_argument("--current", type=Path, default=ROOT / "rvk/shaders/terrain.glsl")
    parser.add_argument("--export", type=Path, help="write standalone reference GLSL functions for a focused GPU fixture")
    args = parser.parse_args()
    snapshot = args.snapshot.read_bytes()
    assert hashlib.sha256(snapshot).hexdigest() == SNAPSHOT_SHA, "reference is not the verified077 release shader"
    old, current = snapshot.decode(), args.current.read_text()
    expected = function(old, "TerrainBase").replace("vec4 TerrainBase(", "vec4 TerrainBase077(", 1)
    assert tokens(function(current, "TerrainBase077")) == tokens(expected), "non-snow colour body drifted from077"
    assert "if(FL.sharedGroundConfig.w>.5&&!NativeCellSnow(m.uv[0]))returnTerrainBase077(image,smp,m);" in tokens(function(current, "TerrainBase"))
    mapping = function(current, "TerrainMapAt")
    assert "boolclassicShared=FL.sharedGroundConfig.w>.5&&!NativeCellSnow(uv);" in tokens(mapping)
    assert "boolnaturalShared=sharedSurface&&!m.smoothSnow&&!NativeCellManufactured(uv)&&!NativeCellIce(uv);" in tokens(mapping)
    assert "smoothSlope" not in mapping
    assert "m.smoothLighting=m.smoothSnow||naturalShared;" in tokens(mapping)
    assert "if(naturalShared||(sharedSurface" in tokens(mapping)
    assert "floatjoinedHeight=naturalShared?fieldHeight:posW.y;" in tokens(mapping)
    assert "constfloatsupport=1.-smoothstep(.97,.9995,abs(nf.y));" in tokens(mapping)
    assert "elseif(naturalShared)m.lightN=nf*length(n);" in tokens(mapping)
    assert "m.uv[1].y+=s*(posW.y-fieldHeight);m.uv[2].y+=s*(posW.y-fieldHeight);" in tokens(mapping)
    assert "m.dx[1].y=m.dx[2].y=-s*fieldDx;" in tokens(mapping)
    assert "m.dy[1].y=m.dy[2].y=-s*fieldDy;" in tokens(mapping)
    assert "m.px.y=fieldDx;m.py.y=fieldDy;" in tokens(mapping)
    main_shader = (args.current.parent / "ffp_main.glsl").read_text()
    assert "if(terrain.smoothLighting)lightingNormal.xyz=terrain.lightN;" in tokens(main_shader)
    assert "SunVisibility(vPosW,lightingNormal.xyz)" in tokens(main_shader)
    assert "if(FL.sharedGroundConfig.w>.5&&!NativeCellSnow(cell))returnown;" in tokens(function(current, "NativeTopFiltered"))
    if args.export:
        args.export.mkdir(parents=True, exist_ok=True)
        refs = []
        for name in ("TerrainMapAt", "NativeTop", "TerrainBaseRaw", "TerrainBase"):
            body = function(old, name)
            for target in ("TerrainMapAt", "NativeTop", "TerrainBaseRaw", "TerrainBase"):
                body = re.sub(r"\b" + target + r"\(", "Reference077" + target + "(", body)
            if name == "TerrainMapAt":
                body = body.replace("TerrainMap m;", "TerrainMap m;\n    m.lightN=n; m.smoothSnow=false; m.smoothLighting=false; m.detailW=vec3(1.,0.,0.);", 1)
                body = body.replace("return m;", "m.detailW=m.w;\n    return m;", 1)
            refs.append(body)
        (args.export / "shared-plus-077-reference.glsl").write_text("\n\n".join(refs) + "\n")
    print(json.dumps({"result": "passed", "snapshot_sha256": SNAPSHOT_SHA,
                      "checks": ["exact077 TerrainBase body", "non-snow dispatch", "continuous natural slope eligibility; classic flat/functional controls",
                                 "original shared tile joints", "enhanced snow and GroundOn remain separate"],
                      "gpu_executed": False, "export": str(args.export) if args.export else None}))


if __name__ == "__main__":
    main()
