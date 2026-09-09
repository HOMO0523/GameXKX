from pathlib import Path
import runpy
import traceback
try:
    runpy.run_path(str(Path(__file__).with_name('gamexxk_apply_texture_budget.py')),run_name='__main__')
except Exception:
    error=traceback.format_exc()
    (Path(__file__).resolve().parents[2]/'Saved/ImageOptimization/last-operation-error.txt').write_text(error,encoding='utf-8')
    raise
