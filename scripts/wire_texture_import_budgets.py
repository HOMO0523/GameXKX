"""Attach the shared save boundary to project UI importers that set uncompressed UI textures."""
from pathlib import Path
import ast
import json

ROOT=Path(__file__).resolve().parents[1]
changed=[]
for path in sorted((ROOT/'Content/Python').glob('gamexxk_*.py')):
    if not (path.name.startswith(('gamexxk_import_','gamexxk_author_','gamexxk_ensure_','gamexxk_reimport_','gamexxk_rebuild_','gamexxk_repair_'))):continue
    source=path.read_text(encoding='utf-8')
    old_call='return save_loaded_asset(*args, **kwargs)'
    if old_call in source:
        output=source.replace(old_call,'return save_loaded_asset(*args, unreal_module=unreal, **kwargs)')
        ast.parse(output);path.write_text(output,encoding='utf-8');changed.append(str(path.relative_to(ROOT)));continue
    old_import='from gamexxk_texture_budget import save_loaded_asset as save_asset_with_texture_budget'
    wrapper='def save_asset_with_texture_budget(*args, **kwargs):\n    from gamexxk_texture_budget import save_loaded_asset\n    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)\n'
    if old_import in source:
        output=source.replace(old_import,wrapper)
        ast.parse(output);path.write_text(output,encoding='utf-8');changed.append(str(path.relative_to(ROOT)));continue
    if 'TC_EDITOR_ICON' not in source or 'unreal.EditorAssetLibrary.save_loaded_asset(' not in source:continue
    tree=ast.parse(source)
    # Place import after module docstring and __future__ declarations.
    after=0
    for node in tree.body:
        if isinstance(node,ast.Expr) and isinstance(node.value,ast.Constant) and isinstance(node.value.value,str) and after==0:
            after=node.end_lineno
        elif isinstance(node,ast.ImportFrom) and node.module=='__future__':after=node.end_lineno
        else:break
    lines=source.splitlines(keepends=True)
    lines.insert(after,'\n'+wrapper+'\n')
    output=''.join(lines).replace('unreal.EditorAssetLibrary.save_loaded_asset(', 'save_asset_with_texture_budget(')
    ast.parse(output)
    path.write_text(output,encoding='utf-8')
    changed.append(str(path.relative_to(ROOT)))
print(json.dumps(dict(updated=len(changed),files=changed)))
