"""Prepare and import final-idle card portraits for PartyDeck and battle UI.

The pipeline owns three visually distinct groups:

* thirteen player/NPC/partner busts, head-calibrated and anchored lower-right;
* three newly generated route emblems (normal, terrain, rare);
* twenty-one enemy portraits, including three real boss cards, anchored lower-left.

Every character and enemy source is the current production ``idle/frame_0000``.
No text is baked into the PNGs.  UE renders card names and enemy intent text with
its composite font so Chinese remains localisable and cannot become raster noise.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    import unreal
except ImportError:  # The deterministic preparation stage runs outside UE.
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PRODUCTION_ROOT = PROJECT_ROOT / "SourceAssets" / "AnimationProcessing" / "Production"
CARD_PORTRAIT_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "card-portraits"
PARTY_DERIVED_ROOT = CARD_PORTRAIT_ROOT / "generated"
ENEMY_DERIVED_ROOT = PROJECT_ROOT / "SourceAssets" / "Battle" / "EnemyCardArt" / "generated"
ROUTE_CHROMA_SHEET = CARD_PORTRAIT_ROOT / "route-source" / "route_icons_v2_chroma.png"

PARTY_DESTINATION_ROOT = "/Game/GameXXK/UI/PartyDeck/CardArt"
ENEMY_DESTINATION_ROOT = "/Game/GameXXK/UI/Battle/EnemyCardArt"

PORTRAIT_SIZE = (171, 205)
FINAL_IDLE_FRAME_SIZE = (512, 512)
ROUTE_SHEET_SIZE = (1254, 1254)
NAME_SAFE_HEIGHT = 34
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


@dataclass(frozen=True)
class Placement:
    mode: str
    anchor: str
    head_box: tuple[int, int, int, int] | None = None
    target_head_center: tuple[int, int] | None = None
    target_head_height: int = 70
    max_size: tuple[int, int] = (165, 166)
    source_region: tuple[int, int, int, int] | None = None


@dataclass(frozen=True)
class PortraitRecord:
    key: str
    asset_name: str
    source: Path
    source_sha256: str
    source_mode: str
    derived_name: str
    derived_root: Path
    destination_root: str
    placement: Placement

    @property
    def derived_path(self) -> Path:
        return self.derived_root / self.derived_name

    @property
    def asset_path(self) -> str:
        return f"{self.destination_root}/{self.asset_name}"


def _production_idle(name: str) -> Path:
    return PRODUCTION_ROOT / name / "frames" / "frame_0000.png"


def _right_head(
    head_box: tuple[int, int, int, int],
    *,
    center_y: int = 88,
    target_height: int = 70,
) -> Placement:
    return Placement(
        "head_anchor",
        "right",
        head_box=head_box,
        target_head_center=(126, center_y),
        target_head_height=target_height,
    )


def _left_fit(*, max_size: tuple[int, int] = (165, 166)) -> Placement:
    return Placement("transparent_fit", "left", max_size=max_size)


def _route(region: tuple[int, int, int, int]) -> Placement:
    return Placement("chroma_region_fit", "right", max_size=(158, 166), source_region=region)


PARTY_PORTRAITS: tuple[PortraitRecord, ...] = (
    PortraitRecord("Hero", "T_CardPortrait_Hero", _production_idle("character_00_hero_idle"), "d6e1bcc2acbce962c5b872ac628c5a86dbcc99466a56d6a6a48ab8bef6a8cc58", "final_idle", "hero.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((190, 62, 289, 177))),
    PortraitRecord("Npc.TusiChief", "T_CardPortrait_Npc_TusiChief", _production_idle("character_07_tusi_chief_idle"), "b7ff042ca3b6c6289ae117b898a607d53223e6aea4cc01dced5fd553f01aa00f", "final_idle", "npc_tusi_chief.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((240, 140, 326, 240), center_y=92)),
    PortraitRecord("Npc.SongJinBao", "T_CardPortrait_Npc_SongJinBao", _production_idle("character_08_song_jin_bao_idle"), "2d4ae4aaab749a064d862ef6f27519b3109d3b80282977190d7e866304976f64", "final_idle", "npc_song_jin_bao.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((244, 68, 326, 177), center_y=92)),
    PortraitRecord("Npc.YueBai", "T_CardPortrait_Npc_YueBai", _production_idle("character_09_yue_bai_idle"), "08c2836b7a7bb39c1a0a24ac2a3cc2bf2c93a0134c0f3539eb9c277d4ec6902b", "final_idle", "npc_yue_bai.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, Placement("transparent_fit", "right", max_size=(164, 168))),
    PortraitRecord("Npc.ZhouGuangZu", "T_CardPortrait_Npc_ZhouGuangZu", _production_idle("character_10_zhou_guang_zu_idle"), "7d2638d3dda907ebe917da5565e601429dcabe0944420c11fc89b1c7570a7564", "final_idle", "npc_zhou_guang_zu.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((244, 106, 330, 213), center_y=94)),
    PortraitRecord("Npc.JinGui", "T_CardPortrait_Npc_JinGui", _production_idle("character_11_jin_gui_idle"), "9e54550dee618b3e8e06181f12270502bb4977586c2088a985865b614b05235d", "final_idle", "npc_jin_gui.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((232, 154, 310, 246), center_y=94)),
    PortraitRecord("Npc.QiongMeiEr", "T_CardPortrait_Npc_QiongMeiEr", _production_idle("character_12_qiong_mei_er_idle"), "5138259d34e5074179f8c4a201295dfe815f177495c49984fc19b1e3b9da4b60", "final_idle", "npc_qiong_mei_er.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((171, 66, 257, 184), center_y=91)),
    PortraitRecord("Role.Blade", "T_CardPortrait_Role_Blade", _production_idle("character_01_blade_idle"), "e0035c4c48be57c9d5229c542c1f6ac3dc29c94d8948dee1f1d3196c8bb45140", "final_idle", "role_blade.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((257, 84, 345, 193))),
    PortraitRecord("Role.Guard", "T_CardPortrait_Role_Guard", _production_idle("character_02_guard_idle"), "4b1519c16e3e67dab5256536ea092545af43fab3a23cc62f17b0b8942f280752", "final_idle", "role_guard.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((252, 107, 327, 194))),
    PortraitRecord("Role.Healer", "T_CardPortrait_Role_Healer", _production_idle("character_03_healer_idle"), "8788ba16ce4f1aaed7ffe72e6bdf32b5b7f2123ebf4b9a70485550f2d6aef26c", "final_idle", "role_healer.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((205, 78, 289, 185))),
    PortraitRecord("Role.Hunter", "T_CardPortrait_Role_Hunter", _production_idle("character_04_hunter_idle"), "52c14d20ec28d552406a722a7ace13abb33c26a93c29b5e4f39bb5e4a36a142c", "final_idle", "role_hunter.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((176, 137, 253, 231))),
    PortraitRecord("Role.Sorcerer", "T_CardPortrait_Role_Sorcerer", _production_idle("character_05_sorcerer_idle"), "0978508ba1ef076673a06825dc86af691c98912beb94a5ab31b726f828ff9acc", "final_idle", "role_sorcerer.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((204, 100, 286, 208))),
    PortraitRecord("Role.FormationMaster", "T_CardPortrait_Role_FormationMaster", _production_idle("character_06_formation_master_idle"), "826db093a1593c5153bc984009b500096196eca2371192c128601db060483bb0", "final_idle", "role_formation_master.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _right_head((228, 108, 318, 218))),
)


ROUTE_PORTRAITS: tuple[PortraitRecord, ...] = (
    PortraitRecord("Route.General", "T_CardPortrait_Route_General", ROUTE_CHROMA_SHEET, "4b4d0c694e76befb961c9a61ea8dc890c49c267cf71b9502e3f107c3b1d4d02b", "route_chroma_sheet", "route_general.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _route((0, 0, 627, 627))),
    PortraitRecord("Route.Terrain", "T_CardPortrait_Route_Terrain", ROUTE_CHROMA_SHEET, "4b4d0c694e76befb961c9a61ea8dc890c49c267cf71b9502e3f107c3b1d4d02b", "route_chroma_sheet", "route_terrain.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _route((627, 0, 1254, 627))),
    PortraitRecord("Route.Rare", "T_CardPortrait_Route_Rare", ROUTE_CHROMA_SHEET, "4b4d0c694e76befb961c9a61ea8dc890c49c267cf71b9502e3f107c3b1d4d02b", "route_chroma_sheet", "route_rare.png", PARTY_DERIVED_ROOT, PARTY_DESTINATION_ROOT, _route((0, 627, 627, 1254))),
)


def _enemy(
    key: str,
    asset_suffix: str,
    production_name: str,
    source_hash: str,
    *,
    boss: bool = False,
) -> PortraitRecord:
    return PortraitRecord(
        key,
        f"T_CardPortrait_Enemy_{asset_suffix}",
        _production_idle(production_name),
        source_hash,
        "final_idle",
        f"{production_name.removesuffix('_idle')}.png",
        ENEMY_DERIVED_ROOT,
        ENEMY_DESTINATION_ROOT,
        _left_fit(max_size=(168, 174) if boss else (165, 166)),
    )


ENEMY_PORTRAITS: tuple[PortraitRecord, ...] = (
    _enemy("Enemy.Ch1.Rooster", "Ch1_Rooster", "enemy_01_rooster_idle", "29d7b94d5faf6aa131f90fa10180d78c04a3be6c6b29d433684b56ebe3cf5b6d"),
    _enemy("Enemy.Ch1.Goat", "Ch1_Goat", "enemy_02_goat_idle", "12480e47540971c299dd3800774c58046815c646d54efcb13a4a4eadedc01cdc"),
    _enemy("Enemy.Ch1.Weasel", "Ch1_Weasel", "enemy_03_weasel_idle", "5745290471ea990fc5d864835176029be4f22fb85f9c145097dc9c52a18e9d44"),
    _enemy("Enemy.Ch1.Civet", "Ch1_Civet", "enemy_04_civet_idle", "ae1e7689b240f5c225b6370f4a647ec529d04ca89cf6aecca89199a27caedbc1"),
    _enemy("Enemy.Ch1.IronfeatherRooster", "Ch1_IronfeatherRooster", "enemy_05_ironfeather_idle", "09b73dc103794190f46ede150a80db7149edc64472250ccf87c15593918bf2d7"),
    _enemy("Enemy.Ch1.BluehornGoatKing", "Ch1_BluehornGoatKing", "enemy_06_bluehorn_idle", "ea6fa6f1a4682451d9b7425bf86bd6cee76c51724d800b192e524655a0764e0b"),
    _enemy("Enemy.Ch1.MoneyRat", "Ch1_MoneyRatBoss", "enemy_19_moneyrat_boss_idle", "ef8f1c9c013b4771158a638928e63862676071307f4c167e3cd02df1aa79e32e", boss=True),
    _enemy("Enemy.Ch2.GrayWolf", "Ch2_GrayWolf", "enemy_07_graywolf_idle", "e5694e980efd9897d41f198c8c3bfa800f83c159a15d1498720e45fcb82d6698"),
    _enemy("Enemy.Ch2.Boar", "Ch2_Boar", "enemy_08_boar_idle", "4252880ad0ae8a17b8657e7e46b0e15689bdd7861d8cedc00e488562f97aa190"),
    _enemy("Enemy.Ch2.Macaque", "Ch2_Macaque", "enemy_09_macaque_idle", "7dcbc9e517b8fb0c9b7fe79b2e380bd823a59f0788a9b1310584bc79192388dd"),
    _enemy("Enemy.Ch2.Porcupine", "Ch2_Porcupine", "enemy_10_porcupine_idle", "1d50138538e15e73de344bee7c34f7b53eaf8003babd4b80d74ecef8a24f7c33"),
    _enemy("Enemy.Ch2.GraymaneWolfKing", "Ch2_GraymaneWolfKing", "enemy_11_graymane_idle", "ce4fb472207d94dd33b9e6e9780e312f627288063e49adb953be37c4f21bfaf3"),
    _enemy("Enemy.Ch2.RedtuskBoarKing", "Ch2_RedtuskBoarKing", "enemy_12_redtusk_idle", "ea96ac0297cf4573b61d94273fd7208bd6b19f3ee1e1ea7e479e2baba8d2d164"),
    _enemy("Enemy.Ch2.BlackBear", "Ch2_BlackBearBoss", "enemy_20_blackbear_boss_idle", "59fe67739b88f4b14f73659e967550dfc5af339a55d89e3cdbcbefd8323060b2", boss=True),
    _enemy("Enemy.Ch3.VenomSnake", "Ch3_VenomSnake", "enemy_13_snake_idle", "973fa46442246325d2d63c595005dd13a1d22ea18e4f31d50f757b29f06e835b"),
    _enemy("Enemy.Ch3.Wildcat", "Ch3_Wildcat", "enemy_14_wildcat_idle", "2316f3ec1c9baaec0f32e663ecabd0a0c3702e6f0ae8bffcd875c8f9860d3ff0"),
    _enemy("Enemy.Ch3.Vulture", "Ch3_Vulture", "enemy_15_vulture_idle", "88267ef3154e66fb74405e3e24d3d9f3b360f0c1f06b2cee7f529c430aab02b0"),
    _enemy("Enemy.Ch3.GiantToad", "Ch3_GiantToad", "enemy_16_toad_idle", "979b7b414f31e93c5604b849c73d99e7dc2d0695678fdf81bf4f91f9f12f80db"),
    _enemy("Enemy.Ch3.WhiteApe", "Ch3_WhiteApe", "enemy_17_whiteape_idle", "955d49afd81d54041907958c6b3f1b6de156162686579e843463f1d80cc40b66"),
    _enemy("Enemy.Ch3.SpiralHornDeer", "Ch3_SpiralHornDeer", "enemy_18_deer_idle", "fe895d9a7db61de3b4d9ad1ec14832580260a1ff97ffbc3dad1e07ea5ee44338"),
    _enemy("Enemy.Ch3.Tiger", "Ch3_TigerBoss", "enemy_21_tiger_boss_idle", "40b744e4ba34c793123b06326cbd12e1282a4dd5d414e42bb0c17f9f41a49c80", boss=True),
)


PORTRAITS = PARTY_PORTRAITS + ROUTE_PORTRAITS + ENEMY_PORTRAITS


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG with an IHDR header: {path}")
    return struct.unpack(">II", header[16:24])


def _png_has_alpha(path: Path) -> bool:
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG with an IHDR header: {path}")
    return header[25] in {4, 6}


def _verify_record(record: PortraitRecord) -> None:
    if not record.source.is_file():
        raise FileNotFoundError(f"card portrait source is missing: {record.source}")
    actual_hash = _sha256(record.source)
    if actual_hash != record.source_sha256:
        raise ValueError(f"card portrait source hash changed for {record.key}: {actual_hash}")
    if record.source_mode == "final_idle":
        if _png_size(record.source) != FINAL_IDLE_FRAME_SIZE or not _png_has_alpha(record.source):
            raise ValueError(f"final idle frame is invalid for {record.key}")
    elif record.source_mode == "route_chroma_sheet":
        if _png_size(record.source) != ROUTE_SHEET_SIZE:
            raise ValueError(f"route icon sheet is invalid for {record.key}")
    else:
        raise ValueError(f"unknown card portrait source mode: {record.source_mode}")


def validate_portrait_plan() -> dict[str, Any]:
    if len(PARTY_PORTRAITS) != 13 or len(ROUTE_PORTRAITS) != 3 or len(ENEMY_PORTRAITS) != 21:
        raise RuntimeError("card portrait plan must contain 13 party, 3 route and 21 enemy records")
    keys: set[str] = set()
    assets: set[str] = set()
    records: list[dict[str, Any]] = []
    for record in PORTRAITS:
        _verify_record(record)
        if record.key in keys or record.asset_path in assets:
            raise RuntimeError(f"duplicate card portrait record: {record.key}")
        keys.add(record.key)
        assets.add(record.asset_path)
        records.append({
            "key": record.key,
            "asset_name": record.asset_name,
            "asset_path": record.asset_path,
            "source_path": record.source,
            "source_mode": record.source_mode,
            "derived_path": record.derived_path,
            "portrait_size": PORTRAIT_SIZE,
            "placement": record.placement,
        })
    return {
        "ok": True,
        "party_count": len(PARTY_PORTRAITS),
        "route_count": len(ROUTE_PORTRAITS),
        "enemy_count": len(ENEMY_PORTRAITS),
        "boss_count": 3,
        "portrait_count": len(PORTRAITS),
        "records": records,
    }


def _remove_chroma_green(image: Any) -> Any:
    from PIL import Image

    rgba = image.convert("RGBA")
    pixels = []
    for red, green, blue, _alpha in rgba.getdata():
        distance = ((red - 0) ** 2 + (green - 255) ** 2 + (blue - 0) ** 2) ** 0.5
        alpha = max(0, min(255, round((distance - 28.0) * 255.0 / 96.0)))
        if alpha < 224 and green > max(red, blue):
            green = min(green, round(max(red, blue) * 1.18))
        pixels.append((red, green, blue, alpha))
    output = Image.new("RGBA", rgba.size)
    output.putdata(pixels)
    return output


def _fit_transparent(source: Any, placement: Placement) -> Any:
    from PIL import Image

    rgba = source.convert("RGBA")
    bbox = rgba.getchannel("A").getbbox()
    if not bbox:
        raise ValueError("transparent card source is empty")
    subject = rgba.crop(bbox)
    subject.thumbnail(placement.max_size, Image.Resampling.LANCZOS)
    output = Image.new("RGBA", PORTRAIT_SIZE, (0, 0, 0, 0))
    if placement.anchor == "left":
        x = 3
    elif placement.anchor == "right":
        x = PORTRAIT_SIZE[0] - subject.width - 3
    else:
        x = (PORTRAIT_SIZE[0] - subject.width) // 2
    output.alpha_composite(subject, (x, PORTRAIT_SIZE[1] - subject.height - 3))
    return output


def _head_calibrated_bust(source: Any, placement: Placement) -> Any:
    from PIL import Image

    if placement.head_box is None or placement.target_head_center is None:
        raise ValueError("head-calibrated portrait has no head calibration")
    rgba = source.convert("RGBA")
    left, top, right, bottom = placement.head_box
    head_height = max(1, bottom - top)
    scale = placement.target_head_height / float(head_height)
    resized = rgba.resize(
        (max(1, round(rgba.width * scale)), max(1, round(rgba.height * scale))),
        Image.Resampling.LANCZOS,
    )
    head_center_x = (left + right) * 0.5
    head_center_y = (top + bottom) * 0.5
    target_x, target_y = placement.target_head_center
    paste_x = round(target_x - head_center_x * scale)
    paste_y = round(target_y - head_center_y * scale)
    output = Image.new("RGBA", PORTRAIT_SIZE, (0, 0, 0, 0))
    output.alpha_composite(resized, (paste_x, paste_y))
    # Names are dynamic UE text.  Keep a deterministic clear strip above the art.
    output.paste((0, 0, 0, 0), (0, 0, PORTRAIT_SIZE[0], NAME_SAFE_HEIGHT))
    return output


def _build_one_portrait(record: PortraitRecord) -> Any:
    from PIL import Image

    with Image.open(record.source) as raw:
        if record.placement.mode == "head_anchor":
            return _head_calibrated_bust(raw, record.placement)
        if record.placement.mode == "transparent_fit":
            return _fit_transparent(raw, record.placement)
        if record.placement.mode == "chroma_region_fit":
            if record.placement.source_region is None:
                raise ValueError(f"route card has no source region: {record.key}")
            region = raw.crop(record.placement.source_region)
            return _fit_transparent(_remove_chroma_green(region), record.placement)
    raise ValueError(f"unsupported card placement mode: {record.placement.mode}")


def _write_contact_sheet(records: tuple[PortraitRecord, ...], destination: Path, columns: int = 4) -> None:
    from PIL import Image, ImageDraw

    cell_width, cell_height = PORTRAIT_SIZE[0] + 24, PORTRAIT_SIZE[1] + 38
    rows = (len(records) + columns - 1) // columns
    sheet = Image.new("RGBA", (columns * cell_width, rows * cell_height), (225, 214, 192, 255))
    draw = ImageDraw.Draw(sheet)
    for index, record in enumerate(records):
        with Image.open(record.derived_path) as portrait:
            x = (index % columns) * cell_width + 12
            y = (index // columns) * cell_height + 22
            sheet.alpha_composite(portrait.convert("RGBA"), (x, y))
            draw.rectangle((x, y, x + PORTRAIT_SIZE[0] - 1, y + PORTRAIT_SIZE[1] - 1), outline=(62, 52, 43, 255), width=1)
            draw.text((x, 4 + (index // columns) * cell_height), record.key, fill=(34, 29, 24, 255))
    destination.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(destination, format="PNG", optimize=True)


def prepare_portrait_sources() -> dict[str, Any]:
    plan = validate_portrait_plan()
    PARTY_DERIVED_ROOT.mkdir(parents=True, exist_ok=True)
    ENEMY_DERIVED_ROOT.mkdir(parents=True, exist_ok=True)
    written: list[str] = []
    for record in PORTRAITS:
        image = _build_one_portrait(record)
        if image.size != PORTRAIT_SIZE:
            raise RuntimeError(f"derived portrait has wrong dimensions for {record.key}: {image.size}")
        image.save(record.derived_path, format="PNG", optimize=True)
        if _png_size(record.derived_path) != PORTRAIT_SIZE or not _png_has_alpha(record.derived_path):
            raise RuntimeError(f"derived portrait could not be verified: {record.derived_path}")
        written.append(str(record.derived_path))
    party_sheet = PARTY_DERIVED_ROOT / "final-idle-bust-contact-sheet.png"
    enemy_sheet = ENEMY_DERIVED_ROOT / "enemy-idle-card-contact-sheet.png"
    _write_contact_sheet(PARTY_PORTRAITS + ROUTE_PORTRAITS, party_sheet)
    _write_contact_sheet(ENEMY_PORTRAITS, enemy_sheet)
    report_path = PARTY_DERIVED_ROOT / "card-portrait-layout-report.json"
    report_path.write_text(json.dumps(_jsonable(plan), ensure_ascii=False, indent=2), encoding="utf-8")
    return {
        **plan,
        "prepared_count": len(written),
        "prepared": written,
        "party_contact_sheet": str(party_sheet),
        "enemy_contact_sheet": str(enemy_sheet),
        "layout_report": str(report_path),
    }


def validate_prepared_portrait_sources() -> dict[str, Any]:
    plan = validate_portrait_plan()
    prepared: list[str] = []
    for record in PORTRAITS:
        if not record.derived_path.is_file():
            raise RuntimeError(f"card portrait derivative is missing: {record.derived_path}")
        if _png_size(record.derived_path) != PORTRAIT_SIZE or not _png_has_alpha(record.derived_path):
            raise RuntimeError(f"card portrait derivative is invalid: {record.derived_path}")
        prepared.append(str(record.derived_path))
    return {**plan, "prepared_count": len(prepared), "prepared": prepared}


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required to import card portrait textures")


def _configure_ui_texture(texture: object) -> None:
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)


def _validate_imported_texture(texture: object, record: PortraitRecord) -> None:
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"card portrait is not a Texture2D: {record.asset_path}")
    if (int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())) != PORTRAIT_SIZE:
        raise RuntimeError(f"card portrait has wrong imported dimensions: {record.asset_path}")
    import_data = texture.get_editor_property("asset_import_data")
    imported_filename = str(import_data.get_first_filename()) if import_data else ""
    if not imported_filename or Path(imported_filename).resolve() != record.derived_path.resolve():
        raise RuntimeError(f"card portrait import source mismatch: {record.asset_path}")


def import_verified_portraits() -> dict[str, Any]:
    _require_unreal()
    prepared = validate_prepared_portrait_sources()
    for destination_root in {record.destination_root for record in PORTRAITS}:
        if not unreal.EditorAssetLibrary.does_directory_exist(destination_root):
            if not unreal.EditorAssetLibrary.make_directory(destination_root):
                raise RuntimeError(f"failed to create card portrait directory: {destination_root}")
    imported: list[str] = []
    reimported: list[str] = []
    for record in PORTRAITS:
        existed = unreal.EditorAssetLibrary.does_asset_exist(record.asset_path)
        task = unreal.AssetImportTask()
        task.filename = str(record.derived_path)
        task.destination_path = record.destination_root
        task.destination_name = record.asset_name
        task.automated = True
        task.save = False
        task.replace_existing = existed
        task.replace_existing_settings = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(record.asset_path)
        if texture is None:
            raise RuntimeError(f"failed to import card portrait: {record.asset_path}")
        _configure_ui_texture(texture)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError(f"failed to save card portrait: {record.asset_path}")
        _validate_imported_texture(texture, record)
        (reimported if existed else imported).append(record.asset_path)
    return {
        **prepared,
        "imported_count": len(imported),
        "reimported_count": len(reimported),
        "imported": imported,
        "reimported": reimported,
    }


def _jsonable(value: Any) -> Any:
    if isinstance(value, Path):
        return str(value)
    if hasattr(value, "__dataclass_fields__"):
        return {field: _jsonable(getattr(value, field)) for field in value.__dataclass_fields__}
    if isinstance(value, dict):
        return {key: _jsonable(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_jsonable(item) for item in value]
    return value


def main(argv: list[str] | None = None) -> dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare", action="store_true", help="Create deterministic local PNG derivatives.")
    parser.add_argument("--execute-import", action="store_true", help="Import previously prepared UE Texture2D assets.")
    args = parser.parse_args(argv)
    if args.execute_import:
        result = import_verified_portraits()
    elif args.prepare:
        result = prepare_portrait_sources()
    else:
        result = validate_portrait_plan()
    print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
