"""Conformance check: every export preset key read by ``package_project.py`` must
exist in the single-source manifest ``configs/export_preset.schema.json``.

This is the Python half of the export-preset schema contract (the C++ half lives in
``engine/test/framework/ExportPresetTest.cpp``). See
``openspec/changes/export-preset-schema-source``.

Run with::

    python python/tests/test_export_preset_schema.py
"""

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
MANIFEST = REPO_ROOT / "configs" / "export_preset.schema.json"
PACKAGE_SCRIPT = REPO_ROOT / "python" / "package_project.py"

# preset.get('key') / preset.get("key") and preset['key'] / preset["key"]
KEY_PATTERNS = (
    re.compile(r"\bpreset\.get\(\s*['\"]([A-Za-z0-9_]+)['\"]"),
    re.compile(r"\bpreset\[\s*['\"]([A-Za-z0-9_]+)['\"]"),
)


class ExportPresetSchemaTest(unittest.TestCase):
    def test_manifest_is_wellformed(self):
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        keys = list(manifest["keys"])
        self.assertTrue(keys, "manifest lists no keys")
        self.assertEqual(len(keys), len(set(keys)), "duplicate keys in manifest")

    def test_cli_keys_are_covered_by_manifest(self):
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        manifest_keys = set(manifest["keys"])
        source = PACKAGE_SCRIPT.read_text(encoding="utf-8")

        referenced = set()
        for pattern in KEY_PATTERNS:
            referenced.update(pattern.findall(source))

        self.assertTrue(referenced, "no preset key access found in package_project.py")
        unknown = sorted(referenced - manifest_keys)
        self.assertEqual(
            unknown,
            [],
            "package_project.py reads preset keys absent from {}: {}".format(MANIFEST, unknown),
        )


if __name__ == "__main__":
    unittest.main()
