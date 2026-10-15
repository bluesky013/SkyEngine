"""Validate the generated game's UIScene metadata without requiring Xcode."""

import contextlib
import importlib.util
import io
from pathlib import Path
import plistlib
import tempfile
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("ios_scene_package_project", ROOT / "python/package_project.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class IOSSceneTemplateTest(unittest.TestCase):
    def test_generated_launcher_declares_one_main_scene_without_storyboards(self):
        with tempfile.TemporaryDirectory() as temp:
            with contextlib.redirect_stdout(io.StringIO()):
                PACKAGE.cmd_create(SimpleNamespace(engine=str(ROOT), dir=temp, name="SceneGame", force=False))
            launcher = Path(temp) / "SceneGame/native/ios"
            with (launcher / "Info.plist").open("rb") as handle:
                metadata = plistlib.load(handle)
            manifest = metadata["UIApplicationSceneManifest"]
            self.assertIs(manifest["UIApplicationSupportsMultipleScenes"], False)
            configurations = manifest["UISceneConfigurations"]
            self.assertEqual(set(configurations), {"UIWindowSceneSessionRoleApplication"})
            main_scenes = configurations["UIWindowSceneSessionRoleApplication"]
            self.assertEqual(len(main_scenes), 1)
            self.assertEqual(main_scenes[0]["UISceneConfigurationName"], "SkyGame")
            self.assertEqual(main_scenes[0]["UISceneClassName"], "UIWindowScene")
            self.assertEqual(main_scenes[0]["UISceneDelegateClassName"], "SkyLauncherSceneDelegate")
            self.assertNotIn("UISceneStoryboardFile", main_scenes[0])
            self.assertNotIn("UIMainStoryboardFile", metadata)


if __name__ == "__main__":
    unittest.main()
