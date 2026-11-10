"""Prepare a local SanX4/Camber release inventory. Never creates/uploads a repo."""
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[1]
version = ET.parse(root / "Directory.Build.props").findtext("PropertyGroup/Version")
build = root / "artifacts" / version
folder = build / "reports/github-release-preparation"
folder.mkdir(parents=True, exist_ok=True)
assets = [build / "windows-installer" / f"Camber-Setup-{version}.exe"]
assets += sorted((build / "android").glob("*-internal.apk"))
if len(assets) != 6 or any(not p.is_file() for p in assets):
    raise SystemExit("Build the Windows installer and all five Android APKs first.")
rows = []
for path in assets:
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    rows.append({"path": str(path.relative_to(root)).replace("\\", "/"),
                 "name": path.name, "bytes": path.stat().st_size, "sha256": digest})
(folder / "SHA256SUMS.txt").write_text("".join(f"{r['sha256']}  {r['name']}\n" for r in rows), encoding="utf-8")
plan = {"repository": "SanX4/Camber", "tag": f"v{version}", "prerelease": True,
        "publicationApproved": False, "notes": f"docs/RELEASE-NOTES-{version}.md",
        "connectionGuide": "docs/CONNECTING.md", "assets": rows,
        "excluded": ["signing material", "pairing data", "private reports", "emulator state", "preservation archives", "unsigned store candidate AAB"],
        "requiredBeforeUpload": "Pass exact-candidate release/security gates and authenticate the authorized GitHub account."}
(folder / "release-plan.json").write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
print(f"Prepared {len(rows)} local release asset hashes for SanX4/Camber v{version}. Nothing uploaded.")
