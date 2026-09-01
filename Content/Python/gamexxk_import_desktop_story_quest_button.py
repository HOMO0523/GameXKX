from __future__ import annotations

import json

from gamexxk_import_desktop_town_toggle import ASSETS, _import_texture


ASSET_NAME = "T_DesktopStoryQuestButton"


def main() -> None:
    texture = _import_texture(ASSET_NAME, ASSETS[ASSET_NAME])
    print(
        json.dumps(
            {
                "status": "PASS",
                "asset_path": texture.get_path_name(),
                "source_sha256": ASSETS[ASSET_NAME],
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
