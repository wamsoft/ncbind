# ncbind — 吉里吉里プラグイン向けネイティブクラスバインダ

Author: miahmie

既存の C++ ライブラリ／クラスを TJS2 から扱えるようにするプラグインを、
引数受け渡しのラッパを手書きせずに作れる C++ テンプレートライブラリ。
型変換はテンプレートによりコンパイラに任せられる。

## 最小例

```cpp
#include "ncbind.hpp"

class MyClass {
public:
    MyClass() {}
    int add(int a, int b) { return a + b; }
};

NCB_REGISTER_CLASS(MyClass) {
    Constructor();
    NCB_METHOD(add);
}
```

```tjs
var o = new MyClass();
Debug.message(o.add(1, 2));   // => 3
invalidate o;
```

## ファイル構成

| ファイル | 内容 |
|---|---|
| `ncbind.hpp` | メインテンプレート (これだけ include すればよい) |
| `ncbind.cpp` | `V2Link` / `V2Unlink` エントリポイント定義 |
| `ncbind.def` | gcc 用エクスポート定義 (VC++ は `/EXPORT:V2Link /EXPORT:V2Unlink`) |
| `ncb_invoke.hpp` | 任意メソッド呼び出し用テンプレート |
| `ncb_foreach.h` | 可変個数マクロ展開用 include マクロ |
| `testbind.cpp` | 各機能の使用例／テスト |

## ドキュメント

- **[ncbind_manual.md](ncbind_manual.md)** — API リファレンス (クラス／メソッド／
  プロパティ／定数登録、RawCallback、Attach、Proxy／Bridge、サブクラス、型変換、
  コールバック、`ncbPropAccessor`、tp_stub 基本 API、ビルド設定、制限事項、
  **単一継承 `NCB_REGISTER_SUBCLASS_OF`**）。まずはこちらを参照。
- **[ncbind_inheritance.md](ncbind_inheritance.md)** — 単一継承 (is-a) サポートの
  設計・エンジン内部の根拠・実装上の注意点。
- **[CLAUDE.md](CLAUDE.md)** — アーキテクチャ概要。

## ビルド

- 対応: gcc / MinGW、VC++2005 以降 (テンプレート実装のため VC++6 は不可)。
  現行 krkrz では MSVC / Clang / gcc でビルドされる。
- プラグイン側は `ncbind.cpp` をリンクする (`V2Link` / `V2Unlink` を提供)。
- 静的プラグインは `TVP_STATIC_PLUGIN` / `TVP_PLUGIN_NAME` を定義。
- CMake は `krkrz_plugin(<name> NCBIND SOURCES ...)` を使う (詳細は manual §19)。
