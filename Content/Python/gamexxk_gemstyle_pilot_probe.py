import json
import unreal

def prop(obj,name):
    try:return obj.get_editor_property(name)
    except Exception as e:return 'unavailable: '+str(e)[:200]

def path(obj):
    try:return obj.get_path_name()
    except Exception:return str(obj)

editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=editor.get_game_world()
report={'editorWorld':path(editor.get_editor_world()),'pieWorld':path(world),'visuals':[]}
report['pythonApi']={n:hasattr(unreal,n) for n in ['WidgetBlueprintLibrary','GameplayStatics','EditorLevelLibrary','GameXXKPlayerController','GameXXKBattleBoardWidget','EditorAssetLibrary']}
print(json.dumps(report,ensure_ascii=False))
if world:
    cls=unreal.load_class(None,'/Script/GameXXK.GameXXKBattleUnitVisualWidget')
    lib=unreal.load_class(None,'/Script/UMG.WidgetBlueprintLibrary')
    report['widgetLibrary']=path(lib)
    report['widgetLibraryMethods']=[x for x in dir(unreal.get_default_object(lib)) if 'widget' in x.lower()] if lib else []
    widgets=[]
    if hasattr(unreal,'WidgetBlueprintLibrary'):widgets=unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(world,cls,False)
    for widget in widgets:
        tex=prop(widget,'atlas_texture');mat=prop(widget,'atlas_material');im=prop(widget,'unit_image')
        report['visuals'].append({'path':path(widget),'atlas':path(tex),'material':path(mat),'image':path(im),'visibility':str(widget.get_visibility())})
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    report['controller']=path(pc)
    report['controllerMethods']=[x for x in dir(pc) if any(s in x for s in ['battle_board','desktop','training','workbench'])]
print(json.dumps(report,ensure_ascii=False))
