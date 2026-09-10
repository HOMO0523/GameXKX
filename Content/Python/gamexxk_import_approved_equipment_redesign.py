"""Replace the user-approved 42 equipment and two material icons, preserving backups."""
import hashlib
import json
import struct
from pathlib import Path
import unreal
import gamexxk_import_gemstyle_items as texture_import

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/Diagnostics/EquipmentRedesign-20260910'
GROUPS=[('equipment','SourceArt/UI/Equipment/gemstyle-redesign-20260910',42),
        ('materials','SourceArt/UI/Items/gemstyle-resources-20260910',2)]

def main():
    rows=[]
    for group,directory,expected in GROUPS:
        art=ROOT/directory
        m=json.loads((art/'manifest.json').read_text(encoding='utf-8'))
        assert m.get('approvedForRuntime') is True
        jobs=json.loads((art/'art-jobs.json').read_text(encoding='utf-8'))['jobs']
        by={j['slug']:j for j in jobs}
        assert len(m['icons'])==expected and len(by)==expected
        for r in m['icons']:
            source=ROOT/r['icon']
            data=source.read_bytes()
            assert hashlib.sha256(data).hexdigest()==r['sha256']
            assert struct.unpack('>II',data[16:24])==(512,512) and data[25]==6
            assert r['asset']==by[r['slug']]['asset']
            assert r['chromaResidualPixels']==0 and r['opaquePixelCount']>5000
            rows.append((group,r,source,art))
    assert len(rows)==44 and len({r[1]['asset'] for r in rows})==44
    OUT.mkdir(parents=True,exist_ok=True)
    report={'expected':44,'imported':[],'complete':False}
    old_output=texture_import.REPORT_ROOT
    try:
        texture_import.REPORT_ROOT=OUT
        for group,r,source,art in rows:
            result=texture_import.import_one(source,r['asset'])
            audit=result['audit']
            assert audit['source_pixels_sha1']==r['sourceBGRASha1'],r['slug']+' decoded pixels differ'
            assert audit['source_read'] and audit['resource_bytes']==262144
            report['imported'].append({'group':group,'slug':r['slug'],'pngSha256':r['sha256'],
                                      'sourcePixelsAndAlphaMatch':True,**result})
            (OUT/'import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    finally:
        texture_import.REPORT_ROOT=old_output
    report['complete']=True
    (OUT/'import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    for group,directory,expected in GROUPS:
        p=ROOT/directory/'manifest.json'
        m=json.loads(p.read_text(encoding='utf-8'))
        for r in m['icons']:r['imported']=True
        m['runtimeImported']=True
        m['importReport']='Saved/Diagnostics/EquipmentRedesign-20260910/import.json'
        p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'ok':True,'imported':44,'allDecodedPixelsAndAlphaMatch':True,'report':str(OUT/'import.json')},ensure_ascii=False))

if __name__=='__main__':main()
