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
    def test_collision_only_root_reaches_audit_and_untyped_root_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            (project / "assets").mkdir()
            world = project / "assets/collision.world"
            mesh = "00000000-0000-0000-0000-000000000001"
            world.write_text(json.dumps({"assetRoots": [{"uuid": mesh, "type": "AuroraMesh"}]}), encoding="utf-8")
            roots, error = PACKAGE.collect_world_asset_roots(project, {})
            self.assertIsNone(error)
            self.assertEqual(roots, [{"uuid": mesh, "type": "AuroraMesh", "world": "collision.world"}])
            with mock.patch.object(PACKAGE, "find_asset_tool", return_value=None):
                self.assertIsNone(PACKAGE.stage_bundles(project, {}, project / "package/products"))
            world.write_text(json.dumps({"assetRoots": [{"uuid": mesh, "type": ""}]}), encoding="utf-8")
            roots, error = PACKAGE.collect_world_asset_roots(project, {})
            self.assertIsNone(roots)
            self.assertIn("Malformed asset root", error)

    def test_jolt_notice_is_staged_with_runtime_configs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            engine, project, destination = root / "engine", root / "project", root / "package/configs"
            (engine / "plugins/jolt").mkdir(parents=True)
            (engine / "plugins/jolt/LICENSE.txt").write_text("Jolt MIT notice", encoding="utf-8")
            PACKAGE.stage_configs(engine, project, destination, development=False)
            self.assertEqual((destination / "licenses/Jolt.txt").read_text(encoding="utf-8"), "Jolt MIT notice")

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
                    mock.patch.object(PACKAGE, "prepare_shader_products", return_value=True), \
                    mock.patch.object(PACKAGE, "stage_bundles"), mock.patch.object(PACKAGE, "package_android") as build:
                PACKAGE._prepare(engine, project, {"platform": "android"}, {}, {}, build=False)
            path = project / "build/android/app/src/main/assets/configs/modules_game.json"
            self.assertTrue(path.is_file())
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")), {"modules": []})
            build.assert_not_called()


if __name__ == "__main__":
    unittest.main()
