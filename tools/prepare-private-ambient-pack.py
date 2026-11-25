"""Prepare existing music-free workshop loops for the opt-in private audio layer."""
import hashlib,json,math,wave
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/environment-078-r65'
OUT=WORK/'audio-runtime-r66';OUT.mkdir(exist_ok=True)
selection={'grid':'grid_network_01','city':'icc_plaza_01','desert':'desert_calm_day_01',
           'forest':'forest_day_01','cold':'shadowlands_penumbra_01','inferno':'shadowlands_inferno_01'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
rows=[];files=[]
for target,name in selection.items():
    source=ROOT/'SoundRework/generated/ambience'/(name+'.wav')
    metadata=json.loads(source.with_suffix('.json').read_text())
    assert sha(source)==metadata['sha256'] and metadata['category']=='ambience' and metadata['loop'] is True
    assert metadata['source_credits']==[] if 'source_credits' in metadata else True
    assert 'no original-game audio or third-party samples used' in metadata['provenance']
    with wave.open(str(source),'rb') as original:
        assert original.getnchannels()==2 and original.getframerate()==48000
        frames=original.getnframes();width=original.getsampwidth();data=original.readframes(frames)
    assert frames==metadata['frames'] and metadata['loop_start_frame']==0 and metadata['loop_end_frame_exclusive']==frames
    if width==3:
        b=np.frombuffer(data,dtype=np.uint8).reshape(-1,3).astype(np.int32)
        raw=b[:,0]|(b[:,1]<<8)|(b[:,2]<<16);raw=(raw^0x800000)-0x800000
        pcm=np.clip(np.rint(raw.astype(float)/256.),-32768,32767).astype('<i2')
        error=int(np.max(np.abs(raw-pcm.astype(np.int32)*256)))
        assert error<=128
    else:
        assert width==2;pcm=np.frombuffer(data,dtype='<i2');error=0
    delivery=OUT/(target+'.wav')
    with wave.open(str(delivery),'wb') as output:
        output.setnchannels(2);output.setsampwidth(2);output.setframerate(48000);output.writeframes(pcm.tobytes())
    with wave.open(str(delivery),'rb') as check:assert check.getnframes()==frames and check.getsampwidth()==2
    peak=float(np.max(np.abs(pcm.astype(np.int32))))/32768.
    assert peak<.5,'Unexpectedly loud source needs review'
    row=dict(region=target,file=delivery.name,sha256=sha(delivery),frames=frames,rate=48000,channels=2,
             source=str(source.relative_to(ROOT)),sourceSha256=metadata['sha256'],sourceBits=width*8,
             peakDbfs=20*math.log10(max(peak,1e-12)),conversionMaxErrorIn24BitLSB=error,
             provenance=metadata['provenance'],processing='Exact frame/channel count; PCM16 delivery conversion only. No gain, resampling, mixing or new synthesis.')
    rows.append(row);files.append(dict(target='randy-vk/audio/ambience/'+delivery.name,source=str(delivery),sha256=sha(delivery)))
manifest=dict(schema=1,privateTest=True,defaultEnabled=False,mode='enhanced regional ambience layer',
              originalEffectsMusicVoicesUnchanged=True,loopCrossfadeSeconds=3,assets=rows,
              listeningOrRetailFitVerified=False)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(OUT/'CREDITS.txt').write_text('AO-VK SoundRework: existing original procedural ambience.\n'
    'Six music-free regional beds from SoundRework/generated/ambience.\n'
    'Original deterministic procedural synthesis; no Funcom audio or third-party samples used.\n'
    'Private opt-in test; no new recordings, compositions or synthesis were produced for this delivery.\n'
    'Master paths, hashes and exact delivery processing are recorded in manifest.json.\n')
for name in ['manifest.json','CREDITS.txt']:
    file=OUT/name;files.append(dict(target='randy-vk/audio/ambience/'+name,source=str(file),sha256=sha(file)))
(WORK/'runtime-assets.json').write_text(json.dumps(dict(schema=1,private=True,defaultEnabled=False,files=files),indent=2)+'\n')
(WORK/'AUDIO-ASSET-RESULTS.json').write_text(json.dumps(dict(result='passed',assets=rows,
    distinctExistingAmbiences=6,runtimeFiles=len(files),newSynthesis=0,sourceFilesChanged=False,
    musicOrVoiceAssetsIncluded=False,retailListeningVerified=False),indent=2)+'\n')
print('Prepared six existing regional ambience beds; exact frames preserved, PCM16 delivery only, default Off.')
