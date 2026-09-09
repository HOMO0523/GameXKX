"""Reuse background-only cleanup for the second requested contour revision."""
import json
import prepare_navigation_bold_revision as prepare

prepare.FOLDER = prepare.ROOT / 'SourceArt/UI/ImageTruth/revisions/20260909-navigation-contour-v2'
prepare.INPUTS = {
    'Warehouse': ('exec-115c8c03-3058-46a8-908e-0c6ba33ae261.png', 'training.nav.warehouse.ink.monochrome.v002'),
    'Training': ('exec-a987c634-61ba-4753-9b30-bc06c33eaa4c.png', 'training.nav.training.ink.v001'),
}
prepare.run()
path = prepare.FOLDER / 'manifest.json'
manifest = json.loads(path.read_text(encoding='utf-8'))
manifest['presentation'].pop('talents_tools_tint_linear', None)
manifest['presentation']['talents_tint_linear'] = [0.01, 0.01, 0.01, 1.0]
manifest['presentation']['tools_tint_linear'] = [0.10, 0.10, 0.10, 1.0]
manifest['images'][0]['reference'] = 'SourceArt/UI/ImageTruth/revisions/20260909-navigation-bold/T_TrainingNavWarehouse.png'
manifest['images'][1]['reference'] = 'SourceArt/UI/ImageTruth/revisions/20260909-training-green/training_nav_training_ink_green_v002.png'
path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
