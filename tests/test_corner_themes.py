import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


REPO = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "generate_corner_art", REPO / "scripts/generate-corner-art.py"
)
GENERATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(GENERATOR)


def make_corner(path: Path, width: int, height: int) -> None:
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.line((1, 1, 1, height - 2, width - 2, height - 2), fill=(0, 0, 0, 255), width=2)
    image.save(path)


class CornerThemeGeneratorTest(unittest.TestCase):
    def test_master_is_deduplicated_and_flipped(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            make_corner(root / "master.png", 20, 16)
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "max_size": 20,
                        "themes": [
                            {"id": "master", "source": "master.png", "seasons": ["all"]}
                        ],
                    }
                )
            )
            output = root / "generated.h"
            GENERATOR.generate(root, manifest, output)
            text = output.read_text()
            self.assertEqual(text.count("constexpr uint8_t kImage"), 1)
            self.assertIn("false, false", text)
            self.assertIn("true, false", text)
            self.assertIn("false, true", text)
            self.assertIn("true, true", text)

    def test_independent_corners_and_partial_override(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            make_corner(root / "master.png", 20, 16)
            make_corner(root / "special.png", 12, 10)
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "max_size": 20,
                        "themes": [
                            {
                                "id": "mixed",
                                "source": "master.png",
                                "corners": {"top_right": "special.png"},
                                "seasons": ["winter"],
                            }
                        ],
                    }
                )
            )
            output = root / "generated.h"
            GENERATOR.generate(root, manifest, output)
            text = output.read_text()
            self.assertEqual(text.count("constexpr uint8_t kImage"), 2)
            self.assertIn("kImage1_special", text)
            self.assertIn("0x01", text)

    def test_project_manifest_covers_every_season_with_daily_choice(self) -> None:
        manifest = json.loads((REPO / "assets/corner-themes/manifest.json").read_text())
        for entry in manifest["themes"]:
            self.assertTrue((REPO / entry["source"]).is_file())

        for season in ("winter", "spring", "summer", "autumn"):
            eligible = [
                entry
                for entry in manifest["themes"]
                if "all" in entry.get("seasons", ["all"])
                or season in entry.get("seasons", [])
            ]
            self.assertGreaterEqual(len(eligible), 2, season)


if __name__ == "__main__":
    unittest.main()
