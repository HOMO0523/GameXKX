# -*- coding: utf-8 -*-
"""Gate-1 art generation for the three battle pages (10/17/11).

Real screenshots are the reference INPUT; gpt-image (via desktop Codex CLI
$imagegen) redesigns each screen into the approved traditional Chinese
ink-wash UI style. Results go to Generated/battle-redesign-20260813/ for
user confirmation.
"""
import glob
import os
import re
import subprocess
import sys
import time

ROOT = 'D:/UE5 demo/GameXXK'
OUT_DIR = ROOT + '/SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/battle-redesign-20260813'
STYLE_SHEET = ROOT + '/tmp/ui_psd_pipeline/style-ref-sheet.png'
LOG_PATH = ROOT + '/tmp/ui_psd_pipeline/battle-redesign-batch.log'

COMMON = (
    'The first image is the current screen of a Chinese wuxia card battle game '
    'that must be redesigned; the second image is the approved UI style reference '
    '(warm ivory xuan-paper panels, deep dark ink linework, low-saturation '
    'gray-brown palette, restrained vermilion accents, subtle paper texture). '
    'Redesign the first image into that refined traditional Chinese ink-wash UI '
    'style. Keep the exact same screen composition, every character position, '
    'HP/qi bars, and every visible Chinese text unchanged. Keep the existing '
    'unified paper-card style of the hand cards exactly as-is. Only improve the '
    'visual style of frames, panels, bars, buttons and the background to match '
    'the reference. No blue RPG frames, no glow gradients, no modern flat design. '
    'Output one 1920x1080 PNG of the full redesigned screen, no watermark, '
    'no logo, no extra text, no extra characters.'
)

JOBS = [
    {
        'name': '10_战斗HUD',
        'input': ROOT + '/Saved/Codex/battle_open_pie.png',
        'extra': 'The screen shows a mountain-river battle background, two enemy '
                 'intent cards at top center, enemy units on the left, hero and '
                 'allied units on the right with HP and qi bars, five hand cards '
                 'at the bottom, a qi orb and an end-turn button at bottom right.',
    },
    {
        'name': '17_战斗HUD_卡牌选中目标',
        'input': ROOT + '/Saved/Codex/battle_target_select_pie.png',
        'extra': 'This is the target-selection state: the middle hand card is '
                 'selected and raised with a warm-gold plus vermilion double '
                 'outline, the target enemy is highlighted with an ink-wash '
                 'ground ring and a small down-pointing marker above its head, '
                 'a red damage preview number appears near the target, the other '
                 'hand cards and the allied units are slightly dimmed.',
    },
    {
        'name': '11_战斗奖励结算',
        'input': ROOT + '/Saved/Codex/battle_reward_pie.png',
        'extra': 'This is the battle reward settlement screen: the battle '
                 'background is dimmed under a warm-gray ink wash, three reward '
                 'cards sit at right-center each with its own icon, a continue '
                 'button is below them, the hero and allied partner units are '
                 'dimmed on the right side, enemy units are absent.',
    },
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


def run_one(codex, job):
    prompt = '$imagegen ' + job['extra'] + ' ' + COMMON + (
        '. Generate the image and then print ONLY the absolute path of the '
        'resulting PNG on the final line of your reply. Do NOT copy, move, '
        'or modify the file.')
    cmd = [codex, 'exec', '--skip-git-repo-check', '--sandbox', 'workspace-write',
           prompt, '-i', job['input'] + ',' + STYLE_SHEET]
    log('start ' + job['name'])
    started = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=420, cwd=ROOT)
    elapsed = int(time.time() - started)
    log(f"done {job['name']} rc={result.returncode} {elapsed}s")
    out = result.stdout or ''
    match = re.search(r'[A-Za-z]:[^\s]+ig_[0-9a-fA-F]+\.png|/[^\s]+ig_[0-9a-fA-F]+\.png', out)
    if match:
        return match.group(0)
    candidates = glob.glob(os.path.expanduser('~/.codex/generated_images/**/ig_*.png'), recursive=True)
    if candidates:
        return max(candidates, key=os.path.getmtime)
    log('  stdout tail: ' + out[-400:])
    return None


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    codex = resolve_codex()
    log('codex: ' + codex)
    failures = []
    for job in JOBS:
        src = run_one(codex, job)
        if not src or not os.path.exists(src):
            failures.append((job['name'], 'no generated image'))
            continue
        dst = OUT_DIR + '/' + job['name'] + '_redesign_v1.png'
        import shutil
        shutil.copy(src, dst)
        log('placed ' + dst)
        time.sleep(2)
    if failures:
        log('FAILURES: ' + str(failures))
        sys.exit(1)
    log('ALL 3 BATTLE REDESIGNS GENERATED')


if __name__ == '__main__':
    main()
