"""Read back the 44 approved runtime icon paths and decoded source pixels."""
import json
from pathlib import Path
import unreal
from gamexxk_import_approved_equipment_redesign import ROOT,OUT,GROUPS

def main():
    rows=[]
    for group,directory,expected in GROUPS:
        m=json.loads((ROOT/directory/'manifest.json').read_text(encoding='utf-8'))
        assert m['runtimeImported'] and len(m['icons'])==expected
        for r in m['icons']:
            t=unreal.load_asset(r['asset']+'.'+r['slug'])
            assert isinstance(t,unreal.Texture2D),r['slug']
            audit=json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(t,True))
            assert audit['source_pixels_sha1']==r['sourceBGRASha1'],r['slug']
            assert [audit['width'],audit['height']]==[512,512] and audit['format']=='BC7'
            assert t.get_editor_property('lod_group')==unreal.TextureGroup.TEXTUREGROUP_UI
            assert not t.get_editor_property('compression_no_alpha')
            assert t.get_editor_property('never_stream')
            files=t.get_editor_property('asset_import_data').extract_filenames()
            assert files and Path(files[0]).resolve()==(ROOT/r['icon']).resolve()
            file=ROOT/'Content'/(r['asset'].removeprefix('/Game/')+'.uasset')
            assert file.exists() and file.stat().st_size>1000
            rows.append({'slug':r['slug'],'objectPath':t.get_path_name(),'sourcePixelsAndAlphaMatch':True,'savedBytes':file.stat().st_size})
    assert len(rows)==44
    report={'ok':True,'verified':len(rows),'records':rows}
    (OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'ok':True,'verified':len(rows),'report':str(OUT/'validation.json')},ensure_ascii=False))

if __name__=='__main__':main()
