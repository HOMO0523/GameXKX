"""Record an explicit visual review against the current file hash; never changes image pixels."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('nodes', nargs='+')
    parser.add_argument('--status', choices=['pass', 'revision_requested'], required=True)
    parser.add_argument('--note', required=True)
    args = parser.parse_args()
    path = ROOT / 'SourceArt/UI/StoryNodes/manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    for node_id in args.nodes:
        entry = manifest['nodes'][node_id]
        actual_hash = hashlib.sha256((ROOT / entry['file']).read_bytes()).hexdigest()
        assert actual_hash == entry['sha256'], f'{node_id}: file changed after generation'
        entry.update(visual_review=args.status, reviewed_sha256=actual_hash, reviewer='agent', review_notes=args.note)
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'reviewed': args.nodes, 'status': args.status, 'pass_count': sum(n.get('visual_review')=='pass' for n in manifest['nodes'].values())}))

if __name__ == '__main__':
    main()
