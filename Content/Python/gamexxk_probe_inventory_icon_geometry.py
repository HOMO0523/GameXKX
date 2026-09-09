import json
import unreal
import gamexxk_probe_real_play_flow as base
world=base._get_game_world()
rows=[]
for widget in unreal.ObjectIterator(unreal.GameXXKInventoryWindowWidget):
    size=unreal.SlateLibrary.get_local_size(widget.get_cached_geometry())
    tree=unreal.find_object(widget,'WidgetTree') or unreal.find_object(widget,'InventoryWindowWidgetTree')
    if not tree:continue
    for index in range(3):
        button=unreal.find_object(tree,f'InventoryBackpackSlot_{index:02d}')
        if not button:continue
        content=button.get_content();bsize=unreal.SlateLibrary.get_local_size(button.get_cached_geometry());csize=unreal.SlateLibrary.get_local_size(content.get_cached_geometry())
        if bsize.x<1:continue
        entries=[]
        if isinstance(content,unreal.PanelWidget):
            for child in content.get_all_children():
                g=child.get_cached_geometry();cs=unreal.SlateLibrary.get_local_size(g)
                item={'name':child.get_name(),'size':[cs.x,cs.y]}
                if isinstance(child,unreal.Image):
                    brush=child.get_editor_property('brush');r=brush.get_editor_property('resource_object');item['resource']=r.get_path_name() if r else None
                entries.append(item)
        rows.append({'widget':widget.get_path_name(),'button':button.get_name(),'button_size':[bsize.x,bsize.y],'content_size':[csize.x,csize.y],'children':entries})
print(json.dumps(rows,ensure_ascii=False))
