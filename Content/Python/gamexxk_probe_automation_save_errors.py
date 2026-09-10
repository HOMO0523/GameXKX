import json
import unreal
records=[]
for mvp in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem):
    error=str(mvp.get_last_save_load_error())
    if error:records.append({'object':mvp.get_path_name(),'error':error})
print(json.dumps(records,ensure_ascii=True))
