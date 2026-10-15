"""Regression coverage for native platform startup configs without build tools."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / "package_project.py"
SPEC = importlib.util.spec_from_file_location("package_project", SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PlatformPackageConfigTest(unittest.TestCase):
    def test_builtin_config_and_project_override_are_staged(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            engine, project, destination = root / "engine", root / "project", root / "assets/configs"
            (engine / "configs").mkdir(parents=True)
            (project / "configs").mkdir(parents=True)
            (engine / "configs/modules_game.json").write_text('{"modules": []}', encoding="utf-8")
            (engine / "configs/default_console.cfg").write_text("builtin", encoding="utf-8")
            (project / "configs/modules_game.json").write_text('{"modules": [{"name": "Game"}]}', encoding="utf-8")
            PACKAGE.stage_configs(engine, project, destination)
            data = json.loads((destination / "modules_game.json").read_text(encoding="utf-8"))
            self.assertEqual(data["modules"], [{"name": "Game"}])
            self.assertEqual((destination / "default_console.cfg").read_text(encoding="utf-8"), "builtin")
            self.assertFalse((project / "configs/default_console.cfg").exists())

    def test_android_prepare_includes_configs_in_apk_assets(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            engine, project = root / "engine", root / "project"
            (engine / "configs").mkdir(parents=True)
            (engine / "configs/modules_game.json").write_text('{"modules": []}', encoding="utf-8")
            with mock.patch.object(PACKAGE, "instantiate_template"), mock.patch.object(PACKAGE, "run_cook"), \
                    mock.patch.object(PACKAGE, "stage_bundles"), mock.patch.object(PACKAGE, "package_android") as build:
                PACKAGE._prepare(engine, project, {"platform": "android"}, {}, {}, build=False)
            path = project / "build/android/app/src/main/assets/configs/modules_game.json"
            self.assertTrue(path.is_file())
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")), {"modules": []})
            build.assert_not_called()


if __name__ == "__main__":
    unittest.main()
