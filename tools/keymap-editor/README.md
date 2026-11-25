# キーマップ編集ツール

`config/cleave_hhjp.keymap` の初期キーマップ（ZMK）をブラウザ上で編集するための単一HTMLツールです。

## 使い方

1. `index.html` をブラウザで開きます（ローカルで直接開くか、GitHub Pages等で配信）。
2. 起動時にGitHub mainの最新 `cleave_hhjp.keymap` を取得します。取得できない環境では、HTMLに埋め込まれた版を使います。
3. レイヤーを選び、キーをクリックして割り当てを変更します。
4. 「全文をコピー」で出力し、`config/cleave_hhjp.keymap` に貼り付けてコミットします。

## 主な機能

- 70キーの物理配置（`cleave_hhjp.dtsi` 準拠）とJIS刻印表示（Macレイヤーは ⌘ / ⌥ 表記）
- `&kp` `&mt` `&lt` `&mo` `&tog` `&to` `&sl` `&sk` `&bt` `&out` `&trans` `&none` と、ファイル内のカスタムbehaviorに対応
- 実キー入力によるキーコード指定
- `&trans` の下位レイヤー値表示
- レイヤー複製時のレイヤー参照付け替え（例: Other0〜2 → Mac0〜2）
- レイヤーの追加・複製・削除・並べ替え・改名（`&mo` `&lt` `&tog` `&to` `&sl` のレイヤー番号は自動で付け替え。behaviorsブロック内の参照は対象外）
- US配列名の記号（`STAR` `PLUS` など）がJISホストで別の文字になる場合の警告
- 編集内容はブラウザのlocalStorageに保存

behaviorsブロックやincludeは読み込んだファイルのまま残し、`keymap` ノードだけを再生成します。

> [!NOTE]
> ZMK Studioで保存した設定がある場合は、書き込み後にRestore Stock Settingsまたは`settings_reset`が必要です。
