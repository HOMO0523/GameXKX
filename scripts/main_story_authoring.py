"""Small authoring helpers; dialogue and illustrations are authored per node."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REF = "SourceAssets/CharacterVisuals/final_selected_v1/"
CHARACTERS = {
    "narrator": {"name": "旁白", "appearance": "", "reference": ""},
    "hero": {"name": "主角", "appearance": "Young adult Chinese male traveler, thick dark eyebrows, black topknot with brown ribbon, brown short jacket, grey-blue crossed tunic and loose trousers, cloth leg wraps, brown shoes, bamboo travel basket and bamboo pole. Match the hero in the campfire reference exactly; no sword or armor.", "reference": REF + "00_hero.png"},
    "you_bai": {"name": "幽白", "appearance": "One compact floating blue-lavender spirit flame with pointed flowing flame silhouette, sharp expressive eyes and smug face. No human body, no arms, no legs, no clothes. Match the blue spirit reference.", "reference": REF + "09_yue_bai.png"},
    "zhou_guang_zu": {"name": "周光祖", "appearance": "Adult Chinese herbal scholar with extremely broad straw hat, black robe, jade-green trousers, long curled black mustache and narrow goatee, bamboo scroll, and one orange fox draped around his shoulders. Match the reference.", "reference": REF + "10_zhou_guang_zu.png"},
    "jin_gui": {"name": "金贵", "appearance": "Slim adult Chinese wanderer with sleepy sly eyes, short black hair, wide straw hat bearing a small red leaf ornament, faded jade-green robe and trousers, waist pouch, long bamboo smoking pipe. Match the reference.", "reference": REF + "11_jin_gui.png"},
    "tusi": {"name": "土司首领", "appearance": "Broad older Chinese mountain chief with an angular broad straw hat, huge white outward-curled mustache and short white goatee, ochre-brown tunic, black trim, large red-orange cape and fork-topped wooden staff. Match the reference.", "reference": REF + "07_tusi_chief.png"},
    "song_jin_bao": {"name": "宋金宝", "appearance": "Lean adult Chinese man with long lively face, mischievous smile, sharply peaked straw hat, red-orange long coat with ochre patterned front panel, brown trousers and waist pouch. Three plain paper slips can be held like a fan; match the reference.", "reference": REF + "08_song_jin_bao.png"},
    "qiong_yao_er": {"name": "琼幺儿", "appearance": "Young adult Chinese mountain woman, dark expressive eyes, black side braid, cream round cap ringed with white beads and red top ornament, red-orange jacket, ochre long skirt, cream waist scarf with trailing red-cream ribbons, small bronze hand bell and green carved bell charm. Match the reference, no new headdress.", "reference": REF + "12_qiong_mei_er.png"},
    "driver": {"name": "老车夫", "appearance": "Weathered middle-aged Chinese coachman, square face, short stubble, dark cloth cap, grey-brown short robe, rope belt and worn cloth shoes. A rolled driving whip at belt. Supporting story character, no vehicle needed.", "reference": ""},
    "mountain_man": {"name": "行脚山客", "appearance": "Thin elderly Chinese mountain traveler, white short beard, plain grey headcloth, faded ochre robe, walking stick and a small cloth bundle of map papers.", "reference": ""},
    "abbot": {"name": "国清寺住持", "appearance": "Kind stout elderly Chinese monk, bald head, long white eyebrows, faded ochre and grey robes, simple wooden prayer beads, friendly knowing expression.", "reference": ""},
    "innkeeper": {"name": "古怪掌柜", "appearance": "Round-faced middle-aged Chinese innkeeper, small neat mustache, indigo cloth headwrap and indigo robe, tan apron, a wooden abacus and a tea cloth.", "reference": ""},
    "woodcutter": {"name": "樵夫", "appearance": "Stocky middle-aged Chinese woodcutter, grey-brown patched short jacket, dark cloth headband, straw sandals, modest firewood bundle and blunt woodcutting axe kept safely aside.", "reference": ""},
    "hunter": {"name": "失足猎人", "appearance": "Lean adult Chinese mountain hunter, plain brown headband, olive-brown short coat, dark trousers and cloth gaiters, modest bow and quiver. Ankle lightly wrapped after rescue. Different person from Zhou Guangzu.", "reference": ""},
    "willow_scholar": {"name": "垂柳先生", "appearance": "Thin elderly Chinese scholar with white eyebrows and short beard, pale grey long robe, dark soft cap, a willow-twig brush and small plain notebook. Calm and dryly humorous.", "reference": ""},
    "boatman": {"name": "船夫", "appearance": "Sturdy middle-aged Chinese river boatman with a short mustache, weathered face, round straw hat, rolled-sleeve blue-grey work robe, dark trousers, a long bamboo pole and a simple wooden skiff.", "reference": ""},
    "villager": {"name": "江湾村民", "appearance": "Friendly adult Chinese farmer in tan-grey homespun clothing, dark headcloth, rolled sleeves and simple cloth shoes, carrying a water bucket or farming tool as required by the scene.", "reference": ""},
    "elder": {"name": "江湾村长", "appearance": "Small elderly Chinese village elder, white short beard, ochre cloth cap and modest brown robe, wooden cane and a carefully wrapped old map scroll.", "reference": ""},
    "poor_traveler": {"name": "老行人", "appearance": "Poor elderly Chinese traveler in a faded blue patched robe and soft grey cap, small cloth food bag, gentle weathered face, dignified and grateful rather than caricatured.", "reference": ""},
    "porter": {"name": "脚夫", "appearance": "Stocky adult Chinese porter with thick uneven eyebrows, short black mustache, brown headcloth, grey-brown short tunic, dusty blue trousers, rope belt and cloth shoulder bag. No straw hat or green robe; clearly distinct from Jin Gui.", "reference": ""},
}

BOX_NODES = {"S00-04", "S01-07", "S02-05", "S03-10", "S04-04", "S05-06"}


def n(key, title, summary, objective, lines, result, scene, *, kind="Dialogue",
      all=(), any=(), optional=False, inside=False, options=(), hints=None, cast=None,
      mood="Lighthearted Chinese wuxia comedy, relaxed relief after a small success.", facts=(), highlight="", camera=""):
    stage = int(key[1:3]) + 1
    speakers = list(dict.fromkeys(speaker for speaker, _ in lines if speaker != "narrator"))
    if hints is None:
        hints = [f"先听清这段交流，留意{objective}。", f"这一步要做的是：{objective}。", f"把本段对话听完并确认结论：{result}"]
    return {
        "id": key, "title": title, "kind": kind, "summary": summary, "script_revision": 2 if highlight else 1,
        "objective": objective, "requires_all": list(all), "requires_any": list(any),
        "optional": optional, "inside_journey": inside,
        "lines": [[a, b] for a, b in lines], "options": list(options),
        "hints": hints, "result": result,
        "reward": {"gold": 100000, "advanced_boxes": 10 if key in BOX_NODES else 0,
                   "normal_boxes": 10 if key in BOX_NODES else 0, "box_level": stage * 5},
        "art": {"node_id": key, "file": key.lower().replace("-", "_") + "_v1.png",
                "display_phase": "story_preview_and_result", "style_version": "clean_ink_v2", "cast": cast if cast is not None else speakers,
                "scene": scene, "mood": mood, "key_moment": highlight, "camera": camera,
                "facts": list(facts) if facts else [objective, result],
                "texture": "/Game/GameXXK/UI/StoryNodes/T_Story_" + key.replace("-", "_") + ".T_Story_" + key.replace("-", "_")},
    }


def option(text, correct, feedback):
    return {"text": text, "correct": correct, "feedback": feedback}


def chapter(key, title, summary, end, nodes):
    # Art-only revisions preserve every dialogue, reward and progression field.
    import json
    direction_path = ROOT / 'SourceAssets/Narrative/MainStory/art-direction-overrides.json'
    directions = json.loads(direction_path.read_text(encoding='utf-8')) if direction_path.exists() else {}
    for node in nodes:
        node['art'].update(directions.get(node['id'], {}))
    return {"id": key, "title": title, "summary": summary,
            "stage_number": int(key[1:]) + 1, "mainline_end": end, "nodes": nodes}
