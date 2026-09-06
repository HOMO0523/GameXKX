"""Import separate readable UI fonts, preserving the user's JiangHu trial assets."""
import copy
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.get_project_file_path()).parent
DEST='/Game/GameXXK/UI/Fonts/Readability'
def main():
    records=[]
    faces={}
    for weight in ('Regular','Bold'):
        source=ROOT/'SourceArt/UI/Fonts/Readability'/('SourceHanSansCN-'+weight+'.otf')
        name='FF_ReadableCJK_'+weight
        path=DEST+'/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            face=unreal.load_asset(path)
            if face.get_class().get_name()!='FontFace' or Path(face.get_editor_property('source_filename')).resolve()!=source.resolve():
                raise RuntimeError('Existing font asset does not match this source: '+path)
        else:
            factory=unreal.FontFileImportFactory()
            factory.set_editor_property('batch_create_font_asset',unreal.BatchCreateFontAsset.CREATE_IF_NO_FONT_EXISTS if weight=='Regular' else unreal.BatchCreateFontAsset.NO)
            task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=DEST;task.destination_name=name
            task.replace_existing=False;task.automated=True;task.save=False;task.factory=factory
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            face=unreal.load_asset(path)
        if not face or face.get_class().get_name()!='FontFace':raise RuntimeError('Font face import failed: '+path)
        if not unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False):raise RuntimeError('Cannot save '+path)
        faces[weight]=face
        records.append({'asset':path,'source':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
    font_path=DEST+'/F_ReadableCJK'
    if not unreal.EditorAssetLibrary.does_asset_exist(font_path):
        created=DEST+'/FF_ReadableCJK_Regular_Font'
        if not unreal.EditorAssetLibrary.rename_asset(created,font_path):raise RuntimeError('Cannot name the new Runtime Font')
    font=unreal.load_asset(font_path)
    if not font or font.get_class().get_name()!='Font':raise RuntimeError('Runtime Font missing')
    data=json.loads(unreal.ToolsetLibrary.get_object_properties(font,['CompositeFont']))
    composite=data['CompositeFont']
    template=composite['defaultTypeface']['fonts'][0]
    entries=[]
    for weight in ('Regular','Bold'):
        entry=copy.deepcopy(template);entry['name']=weight;entry['font']['fontFaceAsset']={'refPath':faces[weight].get_path_name()};entries.append(entry)
    # UE's property setter requires existing elements to remain identical when resizing an array.
    if len(composite['defaultTypeface']['fonts']) == 1:
        composite['defaultTypeface']['fonts']=[entries[0]]
        if not unreal.ToolsetLibrary.set_object_properties(font,json.dumps({'compositeFont':composite})):raise RuntimeError('Cannot configure regular typeface')
    composite['defaultTypeface']['fonts']=entries
    if not unreal.ToolsetLibrary.set_object_properties(font,json.dumps({'compositeFont':composite})):raise RuntimeError('Cannot configure readable typefaces')
    if not unreal.EditorAssetLibrary.save_asset(font_path,only_if_is_dirty=False):raise RuntimeError('Cannot save Runtime Font')
    verify=json.loads(unreal.ToolsetLibrary.get_object_properties(font,['CompositeFont']))
    result={'ok':True,'font':font_path,'faces':records,'composite':verify}
    out=ROOT/'Saved/Codex/RouteUI-20260906/readability-font-import.json';out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False))
main()
