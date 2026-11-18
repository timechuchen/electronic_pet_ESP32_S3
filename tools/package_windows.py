"""Export the built firmware and a minimal Windows flashing ZIP without user data."""
import argparse
import json
from pathlib import Path
import shutil
import zipfile
from windows_flash import ROOT, manifest_from_build, validate_firmware


def package(build):
    manifest, sources = manifest_from_build(build)
    destination = ROOT / "firmware"
    destination.mkdir(exist_ok=True)
    for source, name in sources:
        shutil.copyfile(source, destination / name)
    (destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    validate_firmware(destination)
    output = ROOT / "dist/lanluo-windows-flash.zip"
    output.parent.mkdir(exist_ok=True)
    included = [ROOT / "Windows烧录.bat", ROOT / "Windows烧录说明.md", ROOT / "tools/windows_flash.py",
                destination / "manifest.json", *(destination / name for _, name in sources)]
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in included:
            archive.write(path, "lanluo-windows-flash/" + path.relative_to(ROOT).as_posix())
    downloads = ROOT / "downloads"
    downloads.mkdir(exist_ok=True)
    shutil.copyfile(output, downloads / output.name)
    print(f"固件校验通过，已生成：{output}（{output.stat().st_size:,} 字节）")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    package(parser.parse_args().build_dir)
