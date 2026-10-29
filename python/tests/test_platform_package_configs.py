"""Regression coverage for native platform startup configs without build tools."""

import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

SCRIPT = Path(__file__).resolve().parents[1] / "package_project.py"
SPEC = importlib.util.spec_from_file_location("package_project", SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PlatformPackageConfigTest(unittest.TestCase):
    def test_relative_project_paths_survive_external_tool_working_directories(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temp:
            root = Path(temp)
            engine, project = root / "engine", root / "project"
            (project / "assets").mkdir(parents=True)
            source = project / "assets/main.world"
            source.write_text('{"actors": [], "assetRoots": []}', encoding="utf-8")
            descriptor = project / "Relative.skyproj"
            descriptor.write_text('{"name": "Relative"}', encoding="utf-8")
            (project / "export_presets.json").write_text(json.dumps([
                {"name": "Relative-android", "platform": "android"}
            ]), encoding="utf-8")
            tool = engine / "output/bin/AssetTool"
            tool.parent.mkdir(parents=True)

            def cook(command, cwd):
                argument = Path(command[command.index("--project") + 1])
                self.assertTrue(argument.is_absolute())
                self.assertEqual(argument, project)
                self.assertEqual(Path(cwd), tool.parent)
                self.assertEqual((argument / "assets/main.world").read_bytes(), source.read_bytes())
                return SimpleNamespace(returncode=0)

            for reference in (project, descriptor):
                with self.subTest(reference=reference.name):
                    relative = str(reference.relative_to(Path.cwd()))
                    args = SimpleNamespace(engine=str(engine), project=relative, preset="Relative-android")
                    with mock.patch.object(PACKAGE, "instantiate_template"), \
                            mock.patch.object(PACKAGE, "find_asset_tool", return_value=tool), \
                            mock.patch.object(PACKAGE.subprocess, "run", side_effect=cook) as run, \
                            mock.patch.object(PACKAGE, "prepare_shader_products", return_value=True) as shaders, \
                            mock.patch.object(PACKAGE, "stage_bundles", return_value=["common"]) as bundles, \
                            mock.patch.object(PACKAGE, "package_android") as build:
                        self.assertEqual(PACKAGE.cmd_generate(args), 0)
                        self.assertEqual(PACKAGE.cmd_package(args), 0)
                    self.assertEqual(run.call_count, 2)
                    self.assertEqual(shaders.call_count, 2)
                    self.assertEqual(bundles.call_count, 2)
                    self.assertTrue(all(call.args[1] == project for call in shaders.call_args_list))
                    self.assertTrue(all(call.args[0] == project for call in bundles.call_args_list))
                    self.assertEqual(build.call_args.args[1], project)
                    shader_args = SimpleNamespace(engine=str(engine), project=relative, distribution="Relative-android",
                                                  target=["vulkan"], bundle=None, tool=None, platform="android")
                    with mock.patch.object(PACKAGE, "cook_project_shaders", return_value="") as shader_cook:
                        self.assertEqual(PACKAGE.cmd_cook_shaders(shader_args), 0)
                    self.assertEqual(shader_cook.call_args.args[1], project)

    def test_public_create_keeps_the_project_owned_linux_launcher(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch("builtins.print"):
            PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=Path(temp), name="NativeSmoke", force=False))
            launcher = Path(temp) / "NativeSmoke/native/linux"
            template = PACKAGE.REPO_ROOT / "engine/templates/launcher/linux"
            self.assertEqual((launcher / "main.cpp").read_bytes(), (template / "main.cpp").read_bytes())
            cmake = (launcher / "CMakeLists.txt").read_text(encoding="utf-8")
            self.assertIn('OUTPUT_NAME "NativeSmoke"', cmake)
            self.assertNotIn("{{PROJECT_NAME}}", cmake)

    def test_android_create_and_generate_preserve_system_back_navigation(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch("builtins.print"):
            PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=Path(temp), name="BackSmoke", force=False))
            project = Path(temp) / "BackSmoke"
            with mock.patch.object(PACKAGE, "run_cook", return_value=True), \
                    mock.patch.object(PACKAGE, "prepare_shader_products", return_value=True), \
                    mock.patch.object(PACKAGE, "stage_bundles", return_value=[]):
                self.assertEqual(PACKAGE.cmd_generate(SimpleNamespace(
                    engine=None, project=str(project), preset="BackSmoke-android")), 0)

            namespace = "com.skyengine.backsmoke"
            launcher = project / "native/android/java" / namespace.replace(".", "/") / "SkyEngineActivity.java"
            template = PACKAGE.REPO_ROOT / "engine/templates/launcher/android/java/{{ANDROID_NAMESPACE_PATH}}/SkyEngineActivity.java"
            self.assertEqual(launcher.read_text(encoding="utf-8"), template.read_text(encoding="utf-8")
                             .replace("{{ANDROID_NAMESPACE}}", namespace)
                             .replace("{{LAUNCHER_LIB_NAME}}", "BackSmoke"))
            attributes = "{http://schemas.android.com/apk/res/android}"
            manifest = ET.parse(project / "build/android/app/src/main/AndroidManifest.xml").getroot()
            activity = manifest.find("application/activity")
            self.assertEqual(activity.get(attributes + "name"), namespace + ".SkyEngineActivity")
            self.assertEqual(activity.get(attributes + "launchMode"), "singleTask")
            self.assertEqual(activity.get(attributes + "enableOnBackInvokedCallback"), "true")
            self.assertEqual(activity.find("intent-filter/action").get(attributes + "name"),
                             "android.intent.action.MAIN")
            self.assertEqual(activity.find("intent-filter/category").get(attributes + "name"),
                             "android.intent.category.LAUNCHER")

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
            for reference in ({"uuid": mesh, "type": ""}, {"uuid": "", "type": "AuroraMesh"}):
                with self.subTest(reference=reference):
                    world.write_text(json.dumps({"assetRoots": [reference]}), encoding="utf-8")
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

    def test_default_world_is_published_in_shipping_and_development_startup_config(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            engine, project = root / 'engine', root / 'project'
            (engine / 'configs').mkdir(parents=True)
            (project / 'configs').mkdir(parents=True)
            (project / 'Scene.skyproj').write_text(json.dumps({'defaultScene': './scenes\\main.world'}))
            (project / 'configs/modules_game.json').write_text(json.dumps({
                'modules': [{'name': 'Aurora.ShaderCompiler'}, {'name': 'Game'}],
                'game': {'width': 640, 'height': 480},
            }))
            for development in (False, True):
                destination = root / str(development) / 'configs'
                PACKAGE.stage_configs(engine, project, destination, development=development)
                data = json.loads((destination / 'modules_game.json').read_text())
                self.assertEqual(data['game'], {'width': 640, 'height': 480, 'defaultScene': 'scenes/main.world'})
                self.assertEqual(any(m['name'] == 'Aurora.ShaderCompiler' for m in data['modules']), development)

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
            with mock.patch.object(PACKAGE, "instantiate_template"), mock.patch.object(PACKAGE, "run_cook", return_value=True), \
                    mock.patch.object(PACKAGE, "prepare_shader_products", return_value=True), \
                    mock.patch.object(PACKAGE, "stage_bundles"), mock.patch.object(PACKAGE, "package_android") as build:
                PACKAGE._prepare(engine, project, {"platform": "android"}, {}, {}, build=False)
            path = project / "build/android/app/src/main/assets/configs/modules_game.json"
            self.assertTrue(path.is_file())
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")), {"modules": []})
            build.assert_not_called()


if __name__ == "__main__":
    unittest.main()
