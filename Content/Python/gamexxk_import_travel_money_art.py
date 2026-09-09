"""Import the dedicated gem-style travel-money inventory sprite."""
from pathlib import Path
from gamexxk_import_route_node_art import main

ROOT = Path(__file__).resolve().parents[2]
main(ROOT / "SourceArt/UI/Items/Currency/manifest.json",
     ROOT / "Saved/RouteNodeArt/travel-money-import-report.json")
