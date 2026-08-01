# ncbindtest — ncbind 単一継承 (is-a) 検証プラグイン

ncbind の単一継承拡張 **`NCB_REGISTER_SUBCLASS_OF`** を実機で round-trip 検証する
テスト／サンプルプラグイン（ncbind ライブラリ同梱）。設計は
[`../ncbind_inheritance.md`](../ncbind_inheritance.md)、使い方は
[`../ncbind_manual.md`](../ncbind_manual.md) §21 を参照。

## 検証項目

- 派生は「追加メンバのみ」記述で基底定義 (メソッド／プロパティ) を全継承
- `instanceof` が派生・全祖先で自動的に真
- `tTJSVariant` からの実体取り出しが型 (派生／基底) に応じて正しいポインタを返す
  (`static_cast` アップキャストによりオフセットも正しい)
- 多段継承 (3 段) / メソッド override

## ビルド (任意)

既定ビルドには含めない。検証したいときだけ umbrella の CMake から有効化する。
本フォルダは `src/plugins/ncbind/ncbindtest`（ncbind 内）にあるため、
トップの `CMakeLists.txt` で **プラグイン検索フォルダに ncbind を追加**し、
プラグイン一覧に `ncbindtest` を追加する:

```cmake
list(APPEND TVP_PLUGIN_FOLDERS ${PLUGINS_DIR}/ncbind)   # ncbindtest を探せるように
list(APPEND TVP_PLUGINS ncbindtest)
```

configure 後、当該ターゲットのみビルド:

```bash
cmake --preset x64-windows-win
cmake --build build/x64-windows-win --config Release --target ncbindtest
```

## 実行

`ncbindtest.dll` を `krkrz(64).exe` の隣 (または `plugin/` `plugin64/`) に置き、
本フォルダをデータパスとして渡すと `startup.tjs` が自動実行される。

```bash
cp build/x64-windows-win/core/plugins/ncbindtest/Release/ncbindtest.dll \
   build/x64-windows-win/core/Release/
build/x64-windows-win/core/Release/krkrz64.exe -debug \
   src/plugins/ncbind/ncbindtest
```

`OK` / `FAIL` 行が出力され、最後に `pass=<n> fail=<n>` を表示。
`fail=0` なら `System.exit(0)`。

## ファイル

| ファイル | 内容 |
|---|---|
| `main.cpp` | `TestBase` / `TestDerived` / `TestGrand` / `TestOffChild` と登録・検証用関数 |
| `startup.tjs` | 検証スクリプト (23 アサーション) |
| `CMakeLists.txt` | `krkrz_plugin(ncbindtest NCBIND ...)` |
