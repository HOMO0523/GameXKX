"""Build authored story JSON, a review manuscript and an embedded runtime catalog."""
import argparse
import hashlib
import importlib
import json
from pathlib import Path
from main_story_authoring import ROOT, CHARACTERS


from story_art_style import PAINTERLY_STYLE, STYLE_REFERENCE


def load_chapters():
    return [importlib.import_module(p.stem).CHAPTER for p in sorted((ROOT / "scripts").glob("main_story_s[0-9][0-9].py"))]


def image_request(node):
    monster_refs = node['art'].get('monster_references', [])
    refs = [] if monster_refs else [str(STYLE_REFERENCE)]
    descriptions = []
    portrait_manifest = ROOT / 'SourceArt/UI/StoryPortraits/manifest.json'
    portraits = json.loads(portrait_manifest.read_text(encoding='utf-8')).get('characters', {}) if portrait_manifest.exists() else {}
    for actor in node["art"]["cast"]:
        role = CHARACTERS[actor]
        portrait = portraits.get(actor, {})
        character_reference = role['reference'] or (portrait.get('reference_for_story', '') if portrait.get('review') != 'rejected_by_user' else '')
        if character_reference:
            ref = str(ROOT / character_reference)
            if ref not in refs:
                refs.append(ref)
            descriptions.append(f"{role['name']} ({actor}), identity reference {refs.index(ref) + 1}: preserve this exact face/silhouette/outfit. Use a NEW acting pose for the scene; generic description only where the reference does not resolve it: {role['appearance']}")
        else:
            descriptions.append(f"{role['name']} ({actor}), supporting character: {role['appearance']}")
    style = PAINTERLY_STYLE
    if monster_refs:
        style = style.replace(style.splitlines()[1], 'All supplied images are EXACT project character/enemy identity references, not scene-composition references. Their graphic silhouettes and muted painterly finish are the style authority. Discard magenta backgrounds. Do not copy standing cutout poses.')
    prompt = style + "\nNode: " + node["id"] + " — " + node["title"]
    prompt += "\nNamed cast:\n" + "\n".join(descriptions)
    prompt += "\nExact scene: " + node["art"]["scene"]
    prompt += "\nDramatic key moment: " + node["art"].get("key_moment", "")
    prompt += "\nCamera and composition: " + node["art"].get("camera", "")
    prompt += "\nMood: " + node["art"]["mood"]
    prompt += "\nEventual story outcome (continuity, not a required pose): " + node["result"]
    prompt += "\nRequired story facts: " + " / ".join(node["art"]["facts"])
    for monster in monster_refs:
        refs.append(str(ROOT / monster))
        prompt += f'\nEXACT ENEMY reference {len(refs)} ({Path(monster).stem}): preserve head/ear proportions, graphic silhouette, color blocks and body proportions from this project design, not a generic realistic animal. New physically clear action; no extra limbs or detail.'
    if 'hunter' in node['art']['cast']:
        prompt += '\nHUNTER IDENTITY LOCK: He is CLEAN-SHAVEN, with completely bare upper lip, cheeks and chin. Preserve the dedicated hunter portrait long diamond jaw, wedge nose, asymmetric brows, brown headband, olive coat. NO moustache, beard or stubble, including at distant scale; do not borrow any facial hair from the boatman STYLE reference. His mouth outline is thin and must not become a moustache. The hunter portrait is identity authority.'
    if 'villager' in node['art']['cast']:
        prompt += '\nVILLAGER IDENTITY LOCK: Match the dedicated villager portrait: compact bean-shaped rounded face, dark side-tied headcloth, broad cheerful cheek, clean-shaven tan skin. NO moustache or beard. Do not turn him into the boatman or porter. Keep the exact nose/eye/jaw design at any scale.'
    if 'jin_gui' in node['art']['cast']:
        identity_file = ROOT / 'SourceArt/UI/StoryNodes/Redraw/S03/jin-gui-identity-correction.json'
        if identity_file.exists():
            prompt += '\n' + json.loads(identity_file.read_text(encoding='utf-8'))['lock']
    inn_lore = ROOT / 'SourceAssets/Narrative/World/recurring_inn.json'
    if 'innkeeper' in node['art']['cast'] and inn_lore.exists():
        lore = json.loads(inn_lore.read_text(encoding='utf-8'))
        prompt += '\nWORLD RULE: Every town has the same-looking inn and exactly the same-looking innkeeper. Reuse this approved innkeeper face, headwrap, clothes and the same building/interior design. Only the unique FOUR-CHINESE-CHARACTER inn name changes. Do not explain the mystery as a franchise, twins or magic yet. Change camera/acting for this event, not the architecture or keeper identity. Leave signage blank for a separate text layer.'
        interior = str(ROOT / lore['interior_reference'])
        if interior not in refs and len(refs) < 5:
            refs.append(interior)
            prompt += f' Interior identity reference {len(refs)} fixes the furniture/woodwork design; do not copy its staging or add its other characters.'
    contract = json.dumps({"art": node["art"], "result": node["result"]}, ensure_ascii=False, sort_keys=True)
    return {"node_id": node["id"], "file": node["art"]["file"], "prompt": prompt, "referenced_image_paths": refs,
            "contract_sha256": hashlib.sha256(contract.encode("utf-8")).hexdigest()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--partial", action="store_true")
    parser.add_argument("--image-request")
    args = parser.parse_args()
    chapters = load_chapters()
    nodes = [n for c in chapters for n in c["nodes"]]
    if args.image_request:
        node = next(n for n in nodes if n["id"] == args.image_request)
        print(json.dumps(image_request(node), ensure_ascii=False))
        return
    if not args.partial and (len(chapters) != 6 or len(nodes) != 61):
        raise SystemExit("The final campaign must contain all 6 chapters and 61 nodes")
    data = {"schema_version": 1, "characters": CHARACTERS, "chapters": chapters}
    target = ROOT / "SourceAssets/Narrative/MainStory"
    target.mkdir(parents=True, exist_ok=True)
    text = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    temporary = target / "campaign.json.tmp"
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(target / "campaign.json")
    requests = [image_request(n) for n in nodes]
    (target / "illustration-requests.json").write_text(json.dumps(requests, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if not args.partial:
        raw = json.dumps(data, ensure_ascii=False, separators=(",", ":"))
        generated = ROOT / "Source/GameXXK/Private/Narrative/GameXXKMainStoryGenerated.inl"
        chunks = [raw[i:i + 8000] for i in range(0, len(raw), 8000)]
        generated.write_text('// Generated by scripts/build_main_story_content.py; edit authored chapter modules.\n'
                             'static const TCHAR* const MainStoryJsonChunks[] = {\n' +
                             ',\n'.join('TEXT(R"MAINSTORY(' + chunk + ')MAINSTORY")' for chunk in chunks) + '\n};\n', encoding="utf-8")
    review = ["# 六章主线完整文本与节点合同", "", "风格：诙谐、无厘头的武侠国风；所有任务和图片结果以本文件与 campaign.json 为准。", ""]
    for c in chapters:
        review += ["## " + c["id"] + " " + c["title"], "", c["summary"], ""]
        for node in c["nodes"]:
            reward = node["reward"]
            review += ["### " + node["id"] + " " + node["title"], "", "简介：" + node["summary"], "", "目标：" + node["objective"], "",
                       "类型：" + node["kind"] + "；全部前置：" + ", ".join(node["requires_all"]) + "；任选前置：" + ", ".join(node["requires_any"]), ""]
            review += ["- **" + CHARACTERS[speaker]["name"] + "：** " + line for speaker, line in node["lines"]]
            if node["options"]:
                review += ["", "调查选项：", ""]
                review += ["- " + o["text"] + ("【正确】" if o["correct"] else "【可重试】") + " → " + o["feedback"] for o in node["options"]]
            review += ["", "逐级提示：", ""] + [f"{i+1}. {h}" for i, h in enumerate(node["hints"])]
            review += ["", "结果：" + node["result"], "", f"奖励：金币{reward['gold']}；高级箱{reward['advanced_boxes']}，普通箱{reward['normal_boxes']}，箱级{reward['box_level']}。", "",
                       "插图人物：" + "、".join(CHARACTERS[a]["name"] for a in node["art"]["cast"]), "", "精彩瞬间：" + node["art"].get("key_moment", "待改写"), "", "构图：" + node["art"].get("camera", ""), "", "插图场景：" + node["art"]["scene"], ""]
    (target / "完整剧情与任务节点.md").write_text("\n".join(review), encoding="utf-8")
    print(json.dumps({"chapters": len(chapters), "nodes": len(nodes), "illustrations": len(requests), "partial": args.partial}, ensure_ascii=False))


if __name__ == "__main__":
    main()
