# -*- coding: utf-8 -*-
"""Batch-generate the 6 battle card art drafts via desktop Codex CLI $imagegen.

Gate-1 (art draft) of the 3-step art workflow: luna designed the card art and
emitted imagegen prompts; this script runs gpt-image for each prompt, copies
the result into SourceArt, and fit-resizes to the exact locked card size.
"""
import glob
import os
import re
import subprocess
import sys
import time
from PIL import Image

ROOT = 'D:/UE5 demo/GameXXK'
PROMPT_DIR = ROOT + '/tmp/ui_psd_pipeline/imagegen-prompts'
OUT_DIR = ROOT + '/SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/cards-20260813'
LOG_PATH = ROOT + '/tmp/ui_psd_pipeline/imagegen-batch.log'

TARGETS = [
    ('01_battle_card_normal', 'battle_card_normal.png', 132, 185),
    ('02_battle_card_selected', 'battle_card_selected.png', 132, 185),
    ('03_intent_card', 'intent_card.png', 96, 110),
    ('04_reward_card_scroll', 'reward_card_scroll.png', 206, 285),
    ('05_reward_card_camp', 'reward_card_camp.png', 206, 285),
    ('06_reward_card_formation', 'reward_card_formation.png', 206, 285),
]


def log(msg):
    line = time.strftime('%H:%M:%S') + ' ' + msg
    print(line, flush=True)
    with open(LOG_PATH, 'a', encoding='utf-8') as f:
        f.write(line + '\n')


def resolve_codex():
    ps = ('Get-ChildItem "$env:LOCALAPPDATA/OpenAI/Codex/bin/*/codex.exe" | '
          'Sort-Object { $_.VersionInfo.FileVersion } -Descending | '
          'Select-Object -First 1 -ExpandProperty FullName')
    result = subprocess.run(['pwsh', '-NoProfile', '-Command', ps],
                            capture_output=True, text=True, timeout=60)
    if result.returncode != 0 or not result.stdout.strip():
        raise RuntimeError('cannot resolve desktop codex: ' + result.stderr)
    return result.stdout.strip().splitlines()[-1].strip()


def run_imagegen(codex, prompt, name):
    full_prompt = ('$imagegen ' + prompt +
                   '. Generate the image and then print ONLY the absolute path '
                   'of the resulting PNG on the final line of your reply. '
                   'Do NOT copy, move, or modify the file.')
    cmd = [codex, 'exec', '--skip-git-repo-check', '--sandbox', 'workspace-write', full_prompt]
    log(f'start {name}')
    started = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=ROOT)
    elapsed = int(time.time() - started)
    log(f'done {name} rc={result.returncode} {elapsed}s')
    out = result.stdout or ''
    match = re.search(r'[A-Za-z]:[^\s]+ig_[0-9a-fA-F]+\.png|/[^\s]+ig_[0-9a-fA-F]+\.png', out)
    if match:
        return match.group(0)
    # deterministic fallback: newest generated image
    candidates = glob.glob(os.path.expanduser('~/.codex/generated_images/**/ig_*.png'), recursive=True)
    if candidates:
        return max(candidates, key=os.path.getmtime)
    log(f'  stdout tail: {out[-400:]}')
    return None


def fit_resize(src, dst, w, h):
    im = Image.open(src).convert('RGBA')
    sw, sh = im.size
    target_aspect = w / h
    src_aspect = sw / sh
    if src_aspect > target_aspect:
        # too wide: crop sides
        new_w = int(sh * target_aspect)
        left = (sw - new_w) // 2
        im = im.crop((left, 0, left + new_w, sh))
    else:
        # too tall: crop top/bottom
        new_h = int(sw / target_aspect)
        top = (sh - new_h) // 2
        im = im.crop((0, top, sw, top + new_h))
    im = im.resize((w, h), Image.LANCZOS)
    im.save(dst)
    return dst


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    codex = resolve_codex()
    log('codex: ' + codex)
    failures = []
    for name, out_name, w, h in TARGETS:
        prompt_path = os.path.join(PROMPT_DIR, name + '.txt')
        if not os.path.exists(prompt_path):
            failures.append((name, 'missing prompt file'))
            continue
        with open(prompt_path, encoding='utf-8') as f:
            prompt = f.read().strip()
        src = run_imagegen(codex, prompt, name)
        if not src or not os.path.exists(src):
            failures.append((name, 'no generated image'))
            continue
        dst = os.path.join(OUT_DIR, out_name)
        fit_resize(src, dst, w, h)
        log(f'placed {dst} ({w}x{h})')
        time.sleep(2)
    if failures:
        log('FAILURES: ' + str(failures))
        sys.exit(1)
    log('ALL 6 CARDS GENERATED')


if __name__ == '__main__':
    main()
