#!/usr/bin/env python3
"""GitHub Actions のビルド結果（firmware アーティファクト）を firmware/ に公開する。

使い方: publish_fw.py <アーティファクトを展開したフォルダ> <公開先フォルダ> <コミットSHA>
設定ツール（tools/bt-profiles）は firmware/latest.json を読み、UF2 を直接ドライブへ書き込む。
"""
import datetime
import hashlib
import json
import os
import re
import shutil
import sys

LABELS = [
    (r"^cleave_hhjp_left-btinfo$", "left-btinfo", "左手（BT確認・レイヤ監視用・ZMK Studio対応）", "left"),
    (r"^cleave_hhjp_left-nosd$", "left-nosd", "左手（SoftDeviceなし・RMKから戻す用）", "left"),
    (r"^cleave_hhjp_right-nosd$", "right-nosd", "右手（SoftDeviceなし・RMKから戻す用）", "right"),
    (r"^settings_reset-nosd$", "reset-nosd", "設定リセット（SoftDeviceなし）", "any"),
    (r"^cleave_hhjp_left[ _]rgbled_adapter-", "left", "左手（通常・ZMK Studio対応）", "left"),
    (r"^cleave_hhjp_right[ _]rgbled_adapter-", "right", "右手（通常）", "right"),
    (r"^settings_reset-", "reset", "設定リセット（ペアリング・保存設定を全消去）", "any"),
]


def main(src, dst, sha):
    os.makedirs(dst, exist_ok=True)
    files = []
    for root, _, names in os.walk(src):
        for name in sorted(names):
            if not name.endswith(".uf2"):
                continue
            stem = name[:-4]
            for pat, key, label, side in LABELS:
                if re.search(pat, stem):
                    break
            else:
                key, label, side = re.sub(r"[^A-Za-z0-9_.-]+", "_", stem), stem, "any"
            out = "cleave_hhjp-" + key + ".uf2"
            data = open(os.path.join(root, name), "rb").read()
            open(os.path.join(dst, out), "wb").write(data)
            files.append({
                "key": key, "file": out, "label": label, "side": side,
                "size": len(data), "sha256": hashlib.sha256(data).hexdigest(), "source": name,
            })
    if not files:
        print("UF2 が見つかりません", file=sys.stderr)
        return 1
    keep = {f["file"] for f in files}
    for name in os.listdir(dst):
        if name.endswith(".uf2") and name not in keep:
            os.remove(os.path.join(dst, name))
    order = ["left", "left-btinfo", "right", "reset", "left-nosd", "right-nosd", "reset-nosd"]
    files.sort(key=lambda f: order.index(f["key"]) if f["key"] in order else 99)
    meta = {
        "commit": sha,
        "short": sha[:7],
        "published": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "files": files,
    }
    with open(os.path.join(dst, "latest.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=1)
    print(json.dumps(meta, ensure_ascii=False, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:4]))
