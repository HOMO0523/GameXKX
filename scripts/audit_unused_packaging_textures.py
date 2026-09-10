"""Find unreferenced textures for package exclusion without changing source assets.

First refresh Content/Python/gamexxk_audit_texture_registry.py in UE. Registry
references cover assets; C++ literals and generated path prefixes cover native
dynamic loads. The result is a reviewable list, never an asset deletion.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--registry', type=Path, default=ROOT / 'Saved/ImageOptimization/registry.json')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rules-output', type=Path)
    args = parser.parse_args()
    registry = json.loads(args.registry.read_text(encoding='utf-8-sig'))
    literals, prefixes = set(), set()
    for directory in (ROOT / 'Source', ROOT / 'Plugins/GameXXKDesktopOverlay/Source'):
        for source in directory.rglob('*'):
            if source.suffix not in {'.cpp', '.h', '.inl'} or 'Tests' in source.parts:
                continue
            for value in re.findall(r'"(/Game/[^"\r\n]+)"', source.read_text(encoding='utf-8', errors='replace')):
                if '%' in value:
                    prefixes.add(value.split('%')[0].casefold())
                elif value.endswith('/'):
                    prefixes.add(value.casefold())
                else:
                    package = value.split('.')[0]
                    local = ROOT / 'Content' / package.removeprefix('/Game/')
                    if local.is_dir():
                        prefixes.add(package.casefold() + '/')
                    else:
                        literals.add(package.casefold())
                        # Bare package fragments can be completed by string concatenation.
                        if '.' not in value:
                            prefixes.add(package.casefold())
    # Ini soft-object settings are also runtime roots, but cook directory settings
    # are not evidence that every image in that directory is actually used.
    for source in (ROOT / 'Config').glob('*.ini'):
        for value in re.findall(r'/Game/[^"\s,)]+', source.read_text(encoding='utf-8', errors='replace')):
            literals.add(value.split('.')[0].casefold())

    rows = []
    for row in registry['textures']:
        package = row['package']
        local = ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')
        if (row['asset_class'] != 'Texture2D' or not package.startswith('/Game/GameXXK/')
                or not local.is_file() or row['referencers']):
            continue
        if package.casefold() in literals or any(package.casefold().startswith(prefix) for prefix in prefixes):
            continue
        rows.append({'package': package, 'source_bytes': local.stat().st_size,
                     'referencers': [], 'native_runtime_reference': False})
    rows.sort(key=lambda row: row['package'])
    result = {'generated_at': datetime.now(timezone.utc).isoformat(),
              'registry_modified_at': datetime.fromtimestamp(args.registry.stat().st_mtime, timezone.utc).isoformat(),
              'count': len(rows), 'source_bytes': sum(row['source_bytes'] for row in rows),
              'method': 'Texture2D with no asset referencers and no native dynamic/static or ini reference',
              'excluded_textures': rows}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if args.rules_output:
        lines = ['; Generated from a refreshed registry plus native runtime path audit.',
                 '; Source assets remain untouched. See the packaging acceptance manifest.',
                 '[ExcludeUnreferencedGameXXKImagesFromShipping]', 'Platforms="Windows"',
                 'Targets="Shipping"', 'bExcludeFromPaks=true', 'bOverrideChunkManifest=true']
        for row in rows:
            path = row['package'].replace('/Game/', '.../GameXXK/Content/', 1)
            lines.append(f'+Files="{path}.*"')
        args.rules_output.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(json.dumps({'count': len(rows), 'source_MiB': round(result['source_bytes'] / 2**20, 2),
                      'report': str(args.output)}, ensure_ascii=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
