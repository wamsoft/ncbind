# ncbind プラグイン開発マニュアル

吉里吉里Z (krkrz) プラグイン向けネイティブクラスバインダ `ncbind` の使い方を、
実際のプラグインソースコードに基づいて解説します。

---

## 目次

1. [概要と基本構造](#1-概要と基本構造)
2. [最小プラグインの作り方](#2-最小プラグインの作り方)
3. [クラス登録](#3-クラス登録)
4. [コンストラクタ](#4-コンストラクタ)
5. [メソッド登録](#5-メソッド登録)
6. [プロパティ登録](#6-プロパティ登録)
7. [定数 (Variant) の登録](#7-定数-variant-の登録)
8. [RawCallback による低レベルメソッド](#8-rawcallback-による低レベルメソッド)
9. [グローバル関数の登録](#9-グローバル関数の登録)
10. [既存クラスへの機能追加 (Attach)](#10-既存クラスへの機能追加-attach)
11. [インスタンスフック](#11-インスタンスフック)
12. [Proxy パターン](#12-proxy-パターン)
13. [Bridge パターン](#13-bridge-パターン)
14. [サブクラス登録](#14-サブクラス登録)
15. [型変換のカスタマイズ](#15-型変換のカスタマイズ)
16. [登録前後のコールバック](#16-登録前後のコールバック)
17. [ncbPropAccessor による辞書・配列操作](#17-ncbpropaccessor-による辞書配列操作)
18. [tp_stub の基本型と API](#18-tp_stub-の基本型と-api)
19. [ビルド設定](#19-ビルド設定)
20. [制限事項と注意点](#20-制限事項と注意点)

---

## 1. 概要と基本構造

ncbind は C++ テンプレートを用いて、C++ クラスを TJS2 スクリプトから
使えるように自動的にバインディングを生成するライブラリです。

従来の方式では `tp_stub.h` のマクロ (`TJS_BEGIN_NATIVE_METHOD_DECL` 等) を使って
引数の受け渡しラッパを手で書く必要がありましたが、ncbind を使うと
テンプレートにより型の変換をコンパイラに任せることができます。

### ファイル構成

```
plugins/
  tp_stub/
    tp_stub.h      ... TJS2 プラグインスタブ (型定義・API)
    tp_stub.cpp    ... スタブ実装
  ncbind/
    ncbind.hpp     ... メインテンプレート (これだけ include すればよい)
    ncbind.cpp     ... V2Link / V2Unlink エントリポイント
    ncb_invoke.hpp ... メソッド呼び出しテンプレート
    ncb_foreach.h  ... 可変引数マクロ展開
  yourplugin/
    main.cpp       ... プラグイン本体
```

### 動作の仕組み

ncbind のマクロ (`NCB_REGISTER_CLASS` 等) はすべて **静的初期化子** として展開されます。
プラグインの `V2Link` が呼ばれると `ncbAutoRegister::AllRegist()` により
登録済みのクラスがすべて TJS2 に登録されます。`V2Unlink` 時には逆順で解放されます。

---

## 2. 最小プラグインの作り方

### ステップ 1: C++ クラスを用意する

```cpp
// main.cpp
#include "ncbind.hpp"

class MyClass {
public:
    MyClass() {}
    int add(int a, int b) { return a + b; }
    ttstr hello() { return TJS_W("Hello from C++!"); }
};
```

### ステップ 2: ncbind マクロでクラスを登録する

```cpp
NCB_REGISTER_CLASS(MyClass) {
    Constructor();
    NCB_METHOD(add);
    NCB_METHOD(hello);
}
```

### ステップ 3: ビルドに ncbind.cpp を含める

プラグインのソースに `ncbind.cpp` をリンクします。
`ncbind.cpp` が `V2Link` / `V2Unlink` を提供するので、
プラグイン側で書く必要はありません。

### TJS2 から使う

```tjs
var obj = new MyClass();
Debug.message(obj.add(1, 2));   // => 3
Debug.message(obj.hello());     // => "Hello from C++!"
invalidate obj;
```

---

## 3. クラス登録

### 基本形

```cpp
NCB_REGISTER_CLASS(ClassName) {
    // Constructor, Method, Property 等をここに記述
}
```

`ClassName` がそのまま TJS2 グローバル空間でのクラス名になります。
ブロック内では `Class` という typedef が `ClassName` のエイリアスとして使えます。

### クラス名と C++ クラス名を変える

```cpp
NCB_REGISTER_CLASS_DIFFER(TJSName, CppClassName) {
    // TJS2 側では "TJSName" としてアクセスされる
}
```

### namespace 内のクラスを登録する場合

namespace を含む `::` 付きの名前は直接使えません。typedef で外に出してください。

```cpp
typedef ::Foo::Bar::TargetClass TargetClass;

NCB_REGISTER_CLASS(TargetClass) {
    // ...
}
```

---

## 4. コンストラクタ

### 引数なしコンストラクタ

```cpp
NCB_REGISTER_CLASS(MyClass) {
    Constructor();
}
```

### 引数ありコンストラクタ

```cpp
NCB_REGISTER_CLASS(MyClass) {
    Constructor<int, ttstr>(0);   // MyClass(int, ttstr)
}
```

`(0)` はオーバーロード解決用のダミー引数です。引数ありの場合は必ず付けてください。

### Factory パターン

引数の個数が可変だったり、生成時に特殊な処理が必要な場合は Factory を使います。

**方法 1: RawCallback 形式** (推奨)

```cpp
class MyClass {
public:
    static tjs_error TJS_INTF_METHOD Factory(
        MyClass **inst, tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *objthis)
    {
        if (numparams < 1) return TJS_E_BADPARAMCOUNT;
        *inst = new MyClass(param[0]->GetString());
        return TJS_S_OK;
    }
};

NCB_REGISTER_CLASS(MyClass) {
    Factory(&Class::Factory);
}
```

引数の個数チェックや型変換を自分で行えるため、
省略可能な引数やデフォルト値を実装できます。

**方法 2: Proxy 形式**

```cpp
static MyClass* CreateMyClass(iTJSDispatch2 *objthis, int value) {
    return new MyClass(value);  // NULL を返すとエラー
}

NCB_REGISTER_CLASS(MyClass) {
    Factory(&CreateMyClass);
}
```

第一引数に objthis が渡り、残りの引数が TJS2 から渡されます。

---

## 5. メソッド登録

### 基本形

```cpp
NCB_REGISTER_CLASS(MyClass) {
    // 方法 1: NCB_METHOD マクロ (メソッド名 = TJS2 名)
    NCB_METHOD(doSomething);

    // 方法 2: Method 関数 (TJS2 名を自由に指定)
    Method(TJS_W("doIt"), &Class::doSomething);
    Method("doIt", &Class::doSomething);  // ナロー文字列も可
}
```

### 対応するメソッド型

| C++ メソッド型 | 自動判定 |
|---|---|
| 通常メソッド (`int foo(int)`) | ○ |
| const メソッド (`int foo() const`) | ○ |
| static メソッド (`static int foo()`) | ○ |
| void 返り値 (`void foo()`) | ○ |

### オーバーロードの解決

同名のメソッドが複数ある場合、テンプレートの型推論に失敗します。
`static_cast` または `method_cast` で明示してください。

```cpp
struct MyClass {
    static void Method(int a, int b);
    static void Method(char const *p);
};

NCB_REGISTER_CLASS(MyClass) {
    // method_cast<返り値型, メソッド種別, 引数型...>
    Method("Method1", method_cast<void, Static, int, int>(   &Class::Method));
    Method("Method2", method_cast<void, Static, char const*>(&Class::Method));
}
```

`method_cast` の第2引数 (メソッド種別):
- `Class` — 通常メソッド
- `Const` — const メソッド
- `Static` — static メソッド

`static_cast` でも可能です:
```cpp
static_cast<void (MyClass::*)(int, int)>(&MyClass::Method)
```

---

## 6. プロパティ登録

### 読み書き可能プロパティ

```cpp
NCB_REGISTER_CLASS(MyClass) {
    // 方法 1: NCB_PROPERTY マクロ
    NCB_PROPERTY(value, getValue, setValue);

    // 方法 2: Property 関数
    Property(TJS_W("value"), &Class::getValue, &Class::setValue);
}
```

Getter は引数なしで値を返すメソッド、Setter は値を受け取る void メソッドです。

### 読み取り専用

```cpp
NCB_PROPERTY_RO(name, getName);
// または
Property(TJS_W("name"), &Class::getName, (int)0);
```

### 書き込み専用

```cpp
NCB_PROPERTY_WO(name, setName);
// または
Property(TJS_W("name"), (int)0, &Class::setName);
```

### static プロパティ

static な Getter/Setter もそのまま使えます。TJS2 からは `ClassName.prop` でアクセスします。

```cpp
struct MyClass {
    static int  getCount();
    static void setCount(int n);
};

NCB_REGISTER_CLASS(MyClass) {
    NCB_PROPERTY(count, getCount, setCount);
}
```

---

## 7. 定数 (Variant) の登録

`Variant` を使って定数値をクラスに登録できます。

```cpp
NCB_REGISTER_CLASS(BinaryStream) {
    // インスタンスメンバ定数 (flag = 0)
    // inst.bsRead でアクセス、インスタンスごとに書き換え可能
    Variant(TJS_W("bsRead"),  (tjs_int)TJS_BS_READ,  0);
    Variant(TJS_W("bsWrite"), (tjs_int)TJS_BS_WRITE,  0);

    // static 定数 (flag 省略時 = TJS_STATICMEMBER)
    // BinaryStream.bsRead でアクセス
    Variant(TJS_W("bsRead"),  (tjs_int)TJS_BS_READ);
}
```

第3引数:
- `0` — インスタンスメンバ (各インスタンスに属する)
- `TJS_STATICMEMBER` または省略 — クラスの static メンバ

enum 値の大量登録によく使われます:

```cpp
NCB_REGISTER_CLASS(PSD) {
    Variant("color_mode_rgb",      (int)psd::COLOR_MODE_RGB);
    Variant("blend_mode_normal",   (int)psd::BLEND_MODE_NORMAL);
    Variant("layer_type_normal",   (int)psd::LAYER_TYPE_NORMAL);
    // ...
}
```

---

## 8. RawCallback による低レベルメソッド

引数の個数が可変、省略可能なデフォルト値がある、
複雑な型変換が必要、などの場合は `RawCallback` を使います。

### 基本シグネチャ

**通常形 (iTJSDispatch2* を受け取る)**
```cpp
static tjs_error TJS_INTF_METHOD
myMethod(tTJSVariant *result, tjs_int numparams,
         tTJSVariant **param, iTJSDispatch2 *objthis)
{
    // result  : 戻り値を格納 (NULL の場合は返り値不要)
    // numparams: 引数の個数
    // param   : 引数の配列
    // objthis : 呼び出し元の TJS2 オブジェクト
    if (numparams < 1) return TJS_E_BADPARAMCOUNT;

    ttstr name = param[0]->GetString();
    if (result) *result = tTJSVariant(TJS_W("OK"));
    return TJS_S_OK;
}
```

**ネイティブインスタンス形 (this ポインタを受け取る)**
```cpp
static tjs_error TJS_INTF_METHOD
myMethod(tTJSVariant *result, tjs_int numparams,
         tTJSVariant **param, MyClass *self)
{
    if (!self) return TJS_E_NATIVECLASSCRASH;
    // self を通じてインスタンスにアクセス
    if (result) *result = (tjs_int)self->getValue();
    return TJS_S_OK;
}
```

### 登録

```cpp
NCB_REGISTER_CLASS(MyClass) {
    // 通常メソッドとして登録
    RawCallback(TJS_W("myMethod"), &myMethod, 0);

    // NCB_METHOD_RAW_CALLBACK マクロ版
    NCB_METHOD_RAW_CALLBACK(myMethod, myMethod, 0);

    // static メソッドとして登録
    RawCallback(TJS_W("myStaticMethod"), &myStaticMethod, TJS_STATICMEMBER);
}
```

第3引数:
- `0` — インスタンスメソッド
- `TJS_STATICMEMBER` — static メソッド

### パラメータの操作パターン

```cpp
// 引数を整数として取得 (デフォルト値付き)
int mode = (numparams > 1) ? (tjs_int)*param[1] : 0;

// 引数を文字列として取得
ttstr filename = param[0]->GetString();
// または
const tjs_char *str = param[0]->GetString();

// 引数を実数として取得
double val = (tjs_real)*param[0];

// 引数の型をチェック
if (param[0]->Type() == tvtObject) { /* オブジェクト */ }
if (param[0]->Type() == tvtString) { /* 文字列 */ }

// 返り値を設定
if (result) *result = (tjs_int)42;       // 整数
if (result) *result = ttstr(TJS_W("OK")); // 文字列
if (result) result->Clear();              // void
```

---

## 9. グローバル関数の登録

クラスに属さないグローバル関数を TJS2 に登録します。

```cpp
// 通常関数
static void myGlobalFunc(int a, char const *b) {
    // ...
}

// RawCallback 形式
static tjs_error TJS_INTF_METHOD
myRawFunc(tTJSVariant *result, tjs_int numparams,
          tTJSVariant **param, iTJSDispatch2 *objthis)
{
    return TJS_S_OK;
}

NCB_REGISTER_FUNCTION(MyFunc1, myGlobalFunc);
NCB_REGISTER_FUNCTION(MyFunc2, myRawFunc);
```

TJS2 からは `MyFunc1(123, "abc")` のように呼び出せます。

---

## 10. 既存クラスへの機能追加 (Attach)

吉里吉里の既存クラス (`Layer`, `Pad`, `Array`, `Dictionary`, `Storages` 等) に
メソッドやプロパティを追加できます。

### NCB_ATTACH_CLASS

```cpp
// Storages クラスにメソッドを追加する例
struct StoragesMemFile {
    static bool isExistMemoryFile(ttstr name) { /* ... */ }
    static bool deleteMemoryFile(ttstr name)  { /* ... */ }
};

NCB_ATTACH_CLASS(StoragesMemFile, Storages) {
    NCB_METHOD(isExistMemoryFile);
    NCB_METHOD(deleteMemoryFile);
};
```

- 第1引数: C++ クラス名
- 第2引数: 追加先の TJS2 クラス名
- インスタンスは最初にメソッドが呼ばれた時に引数なしで new される
- コンストラクタは登録不可

### NCB_ATTACH_FUNCTION

既存クラスに関数を1つだけ追加する簡易版です。

```cpp
static void myFunc(int d) { /* ... */ }

NCB_ATTACH_FUNCTION(Func1, Pad, myFunc);
```

RawCallback 形式の関数も使えます:

```cpp
static tjs_error TJS_INTF_METHOD
rawFunc(tTJSVariant *result, tjs_int numparams,
        tTJSVariant **param, iTJSDispatch2 *objthis)
{ /* ... */ }

NCB_ATTACH_FUNCTION(Func2, Pad, rawFunc);
```

### 静的メソッドとして追加

```cpp
NCB_ATTACH_CLASS(DictAdd, Dictionary) {
    RawCallback("saveStruct2", &DictAdd::saveStruct2, TJS_STATICMEMBER);
};
```

---

## 11. インスタンスフック

`NCB_GET_INSTANCE_HOOK` を使うと、TJS2 からネイティブインスタンスを取得する
処理をカスタマイズできます。遅延初期化やメソッド呼び出し前後のフック処理に使います。

### 定義

```cpp
NCB_GET_INSTANCE_HOOK(MyClass)
{
    // コンストラクタ (省略可能)
    NCB_GET_INSTANCE_HOOK_CLASS () {}

    // インスタンスゲッタ — メソッド呼び出しのたびに呼ばれる
    NCB_INSTANCE_GETTER(objthis) {
        ClassT* obj = GetNativeInstance(objthis);
        if (!obj) {
            // 初回アクセス時にインスタンスを生成
            obj = new ClassT(objthis);
            SetNativeInstance(objthis, obj);
        }
        // 毎回の前処理
        obj->reset();
        return obj;
    }

    // デストラクタ — メソッド呼び出し後に呼ばれる
    ~NCB_GET_INSTANCE_HOOK_CLASS () {
        // 後処理をここに書ける
    }

private:
    iTJSDispatch2 *_objthis;
    ClassT        *_obj;
};   // class 定義なのでセミコロン必須
```

### NCB_ATTACH_CLASS_WITH_HOOK と組み合わせる

既存クラスに機能を追加する際、インスタンスの生成タイミングを制御したい場合に使います。
**NCB_GET_INSTANCE_HOOK は NCB_ATTACH_CLASS_WITH_HOOK より前に定義してください。**

```cpp
// 1. フックを定義
NCB_GET_INSTANCE_HOOK(LayerExDraw)
{
    NCB_INSTANCE_GETTER(objthis) {
        ClassT* obj = GetNativeInstance(objthis);
        if (!obj) {
            obj = new ClassT(objthis);
            SetNativeInstance(objthis, obj);
        }
        obj->reset();
        return obj;
    }
    ~NCB_GET_INSTANCE_HOOK_CLASS () {}
};

// 2. フック付きでアタッチ
NCB_ATTACH_CLASS_WITH_HOOK(LayerExDraw, Layer) {
    NCB_METHOD(drawRectangle);
    NCB_METHOD(clear);
    NCB_PROPERTY(smoothingMode, getSmoothingMode, setSmoothingMode);
};
```

`NCB_REGISTER_CLASS` で登録したクラスにも適用されます。

---

## 12. Proxy パターン

クラス外の static 関数をクラスメソッドのように振る舞わせます。
既存ライブラリに手を加えずに追加の処理を入れたい場合に有効です。

### メソッドの Proxy

```cpp
// Proxy 関数: 第1引数にインスタンスポインタが渡る
static ttstr GetTagProxy(MyClass *inst) {
    return ttstr("Proxy:") + inst->getTag();
}

NCB_REGISTER_CLASS(MyClass) {
    Method("getTagProxy", &GetTagProxy, Proxy);
}
```

### プロパティの Proxy

```cpp
static int getValueProxy(MyClass *inst) {
    return inst->getValue() * 2;
}
static void setValueProxy(MyClass *inst, int v) {
    inst->setValue(v / 2);
}

NCB_REGISTER_CLASS(MyClass) {
    Property("scaledValue", &getValueProxy, &setValueProxy, Proxy);
}
```

---

## 13. Bridge パターン

あるクラスのメソッド呼び出しを、そのクラスが内部に保持している
別のクラスのインスタンスに委譲します。

### Bridge ファンクタの定義

```cpp
struct MyClass {
    PropertyTest prop;      // 内部に保持するインスタンス
    TypeConvChecker tc;
};

// ファンクタ: MyClass から PropertyTest を取り出す
struct BridgeToProp {
    PropertyTest* operator()(MyClass *p) const {
        return &(p->prop);
    }
};

struct BridgeToTC {
    TypeConvChecker* operator()(MyClass *p) const {
        return &(p->tc);
    }
};
```

### 登録

```cpp
NCB_REGISTER_CLASS(MyClass) {
    // PropertyTest のメソッドを MyClass のメソッドとして登録
    Method("bridgeSInt", &TypeConvChecker::SInt, Bridge<BridgeToTC>());

    // プロパティも同様
    Property("bridgeProp", &PropertyTest::Get, &PropertyTest::Set, Bridge<BridgeToProp>());
}
```

### ProxyBridge / BridgeProxy

Proxy と Bridge を組み合わせることもできます。

```cpp
static int sIntProxy(TypeConvChecker *tc, int n) {
    return tc->SInt(n);
}

NCB_REGISTER_CLASS(MyClass) {
    // Proxy 関数を Bridge 経由で呼ぶ
    Method("bridgeSIntProxy", &sIntProxy, ProxyBridge<BridgeToTC>());
}
```

---

## 14. サブクラス登録

クラスの中に階層構造を持たせたい場合に使います。
TJS2 からは `ParentClass.SubClassName` としてアクセスします。

### サブクラスの定義

```cpp
struct Header {
    void store() { /* ... */ }
};

struct Item {
    Item(int id) : _id(id) {}
    int _id;
};

NCB_REGISTER_SUBCLASS(Header) {
    Constructor();
    Method(TJS_W("store"), &Class::store);
}

NCB_REGISTER_SUBCLASS(Item) {
    Constructor<int>(0);
}
```

### 親クラスに組み込む

```cpp
NCB_REGISTER_CLASS(MyDialog) {
    Constructor();

    // NCB_SUBCLASS(TJS2名, C++クラス名)
    NCB_SUBCLASS(Header, Header);
    NCB_SUBCLASS(Item, Item);

    NCB_METHOD(open);
}
```

TJS2 での使用:
```tjs
var dlg = new MyDialog();
var hdr = new MyDialog.Header();
hdr.store();
var item = new MyDialog.Item(42);
```

---

## 15. 型変換のカスタマイズ

ncbind は C++ の基本型 (int, double, bool, char const*, ttstr 等) と
tTJSVariant の相互変換を自動で行います。
それ以外の型を引数や返り値に使う場合は型変換を登録する必要があります。

### NCB_TYPECONV_CAST — キャスト変換

```cpp
// Type を CastType にキャストして tTJSVariant と変換する
NCB_TYPECONV_CAST(MyType, tTVInteger);
```

### NCB_TYPECONV_CAST_INTEGER — 整数型変換

enum などを整数として扱う場合の簡易版です。

```cpp
NCB_TYPECONV_CAST_INTEGER(psd::LayerType);
NCB_TYPECONV_CAST_INTEGER(psd::BlendMode);
```

### NCB_SET_CONVERTOR — カスタムコンバータ

双方向の変換を完全に自前で実装する場合に使います。

```cpp
struct PointF { float x, y; };

struct PointFConvertor {
    // tTJSVariant → PointF (TJS2 から C++ へ)
    void operator()(PointF &dst, const tTJSVariant &src) {
        if (src.Type() == tvtObject) {
            // 配列 [x, y] またはオブジェクト {x:, y:} として受け取る
            ncbPropAccessor info(src);
            dst.x = (float)info.getRealValue(0);
            dst.y = (float)info.getRealValue(1);
        } else {
            dst = PointF();
        }
    }

    // PointF → tTJSVariant (C++ から TJS2 へ)
    void operator()(tTJSVariant &dst, const PointF &src) {
        // 配列として返す
        iTJSDispatch2 *arr = TJSCreateArrayObject();
        ncbPropAccessor acc(arr, false);
        acc.SetValue(0, (double)src.x);
        acc.SetValue(1, (double)src.y);
        dst = tTJSVariant(arr, arr);
        arr->Release();
    }
};

NCB_SET_CONVERTOR(PointF, PointFConvertor);
```

登録後は `PointF` を普通にメソッドの引数・返り値に使えます:

```cpp
struct MyClass {
    void setPosition(PointF pos) { /* ... */ }
    PointF getPosition() { /* ... */ }
};

NCB_REGISTER_CLASS(MyClass) {
    NCB_METHOD(setPosition);   // TJS2 の配列/辞書を PointF に自動変換
    NCB_METHOD(getPosition);   // PointF を TJS2 の配列に自動変換
}
```

### 部分的な変換の登録

```cpp
// tTJSVariant → Type のみ登録
NCB_SET_TOVALUE_CONVERTOR(Type, Convertor);

// Type → tTJSVariant のみ登録
NCB_SET_TOVARIANT_CONVERTOR(Type, Convertor);
```

### 登録済みクラスのインスタンスを引数に取る

ncbind で登録したクラスのインスタンスは、自動的に Boxing/Unboxing されます。
そのクラスのポインタ・参照をそのまま引数に使えます。

```cpp
struct BoxTest {
    static bool check(TypeConvChecker const &ref, bool b) {
        return ref.Bool(b);
    }
};
```

| 返し方 | 動作 |
|---|---|
| コピー (`T`) | コピーコンストラクタで新規インスタンス生成 |
| 参照 (`T&`) | TJS2 へ渡すが invalidate 時に delete されない |
| ポインタ (`T*`) | TJS2 へ渡し invalidate 時に delete される |

---

## 16. 登録前後のコールバック

プラグインの読み込み・解放時に処理を実行できます。

```cpp
static void onPreRegist() {
    TVPAddImportantLog(ttstr("Plugin loaded"));
}

static void onPostRegist() {
    // クラス登録完了後の初期化
    // 例: TJS2 のグローバルオブジェクトを取得しておく
    tTJSVariant var;
    TVPExecuteExpression(TJS_W("Date"), &var);
    dateClass = var.AsObject();
}

static void onPreUnregist() {
    // クラス解放前のクリーンアップ
    if (dateClass) { dateClass->Release(); dateClass = NULL; }
}

static void onPostUnregist() {
    // すべて解放された後の後始末
}

NCB_PRE_REGIST_CALLBACK(onPreRegist);
NCB_POST_REGIST_CALLBACK(onPostRegist);
NCB_PRE_UNREGIST_CALLBACK(onPreUnregist);
NCB_POST_UNREGIST_CALLBACK(onPostUnregist);
```

呼び出し順序:

```
V2Link 時:
  PRE_REGIST_CALLBACK    → 各クラス登録 → POST_REGIST_CALLBACK

V2Unlink 時:
  PRE_UNREGIST_CALLBACK  → 各クラス解放 → POST_UNREGIST_CALLBACK
```

同じ種類のコールバックが複数登録された場合の実行順序は保証されません。

### グローバル定数の登録 (コールバックを活用)

```cpp
static void PostRegistCallback() {
    iTJSDispatch2 *global = TVPGetScriptDispatch();
    if (global) {
        tTJSVariant val((tjs_int)42);
        global->PropSet(TJS_MEMBERENSURE|TJS_IGNOREPROP,
                        TJS_W("MY_CONSTANT"), NULL, &val, global);
        global->Release();
    }
}

static void PreUnregistCallback() {
    iTJSDispatch2 *global = TVPGetScriptDispatch();
    if (global) {
        global->DeleteMember(0, TJS_W("MY_CONSTANT"), NULL, global);
        global->Release();
    }
}

NCB_POST_REGIST_CALLBACK(PostRegistCallback);
NCB_PRE_UNREGIST_CALLBACK(PreUnregistCallback);
```

---

## 17. ncbPropAccessor による辞書・配列操作

`ncbPropAccessor` は TJS2 の辞書 (Dictionary) や配列 (Array) を
C++ から操作するためのラッパクラスです。

### 配列の操作

```cpp
void processArray(iTJSDispatch2 *arrObj) {
    ncbPropAccessor arr(arrObj);

    // 要素数の取得
    tjs_int count = arr.GetArrayCount();

    // 値の取得 (インデックス指定)
    int    intVal = arr.getIntValue(0);         // デフォルト値: 0
    double dblVal = arr.getRealValue(1, 1.0);   // デフォルト値指定
    ttstr  strVal = arr.getStrValue(2);

    // テンプレート版
    int v = arr.GetValue(0, ncbTypedefs::Tag<int>());

    // 値の設定
    arr.SetValue(0, (int)42);
    arr.SetValue(1, ttstr(TJS_W("hello")));

    // 存在チェック
    if (arr.HasValue(3)) { /* ... */ }
}
```

### 辞書の操作

```cpp
void processDict(iTJSDispatch2 *dictObj) {
    ncbPropAccessor dict(dictObj);

    // 値の取得 (キー指定)
    int    width  = dict.getIntValue(TJS_W("width"));
    ttstr  name   = dict.getStrValue(TJS_W("name"));
    double scale  = dict.getRealValue(TJS_W("scale"), 1.0);

    // 値の設定
    dict.SetValue(TJS_W("result"), (int)100);

    // 存在チェック
    if (dict.HasValue(TJS_W("optional"))) { /* ... */ }
}
```

### 新規作成

```cpp
// 配列を新規作成
ncbArrayAccessor arr;
arr.SetValue(0, (int)1);
arr.SetValue(1, (int)2);
arr.SetValue(2, (int)3);
// arr.GetDispatch() で iTJSDispatch2* を取得

// 辞書を新規作成
ncbDictionaryAccessor dict;
dict.SetValue(TJS_W("x"), (int)100);
dict.SetValue(TJS_W("y"), (int)200);
```

### RawCallback での活用

```cpp
static tjs_error TJS_INTF_METHOD
myMethod(tTJSVariant *result, tjs_int numparams,
         tTJSVariant **param, MyClass *self)
{
    if (numparams < 1) return TJS_E_BADPARAMCOUNT;

    // 辞書引数を受け取る
    if (param[0]->Type() == tvtObject) {
        ncbPropAccessor options(*param[0]);
        int width  = options.getIntValue(TJS_W("width"), 640);
        int height = options.getIntValue(TJS_W("height"), 480);
        self->resize(width, height);
    }

    // 配列を返す
    if (result) {
        ncbArrayAccessor arr;
        arr.SetValue(0, self->getX());
        arr.SetValue(1, self->getY());
        iTJSDispatch2 *d = arr.GetDispatch();
        *result = tTJSVariant(d, d);
    }
    return TJS_S_OK;
}
```

### FuncCall で TJS2 メソッドを呼ぶ

```cpp
ncbPropAccessor obj(dispatch);

// 引数なし
tTJSVariant ret;
obj.FuncCall(0, TJS_W("methodName"), NULL, &ret);

// 引数あり (tTJSVariant を直接渡す)
tTJSVariant p1((tjs_int)42), p2(TJS_W("hello"));
obj.FuncCall(0, TJS_W("methodName"), NULL, &ret, p1, p2);
```

---

## 18. tp_stub の基本型と API

### 基本型

| 型名 | 内容 |
|---|---|
| `tjs_int` | プラットフォーム整数 |
| `tjs_int32` | 32ビット整数 |
| `tjs_int64` | 64ビット整数 |
| `tjs_uint32` | 32ビット符号なし整数 |
| `tjs_real` | 実数 (double) |
| `tjs_char` | ワイド文字 (wchar_t / char16_t) |
| `tjs_nchar` | ナロー文字 (char) |
| `ttstr` | TJS2 文字列クラス (tTJSString) |
| `tTJSVariant` | 汎用値型 (整数/実数/文字列/オブジェクト/オクテット/void) |
| `iTJSDispatch2` | TJS2 オブジェクトインターフェース |

### tTJSVariant の型判定

```cpp
tTJSVariantType type = var.Type();
// tvtVoid    — 空
// tvtObject  — オブジェクト
// tvtString  — 文字列
// tvtOctet   — バイナリ
// tvtInteger — 整数
// tvtReal    — 実数
```

### tTJSVariant の値取得

```cpp
tjs_int64       i = var.AsInteger();      // 整数として
double          d = (tjs_real)var;         // 実数として
const tjs_char *s = var.GetString();      // 文字列として
iTJSDispatch2  *o = var.AsObjectNoAddRef(); // オブジェクトとして (参照カウント増加なし)
iTJSDispatch2  *o = var.AsObject();       // オブジェクトとして (要 Release)
```

### iTJSDispatch2 の主要メソッド

```cpp
// メソッド呼び出し
obj->FuncCall(0, TJS_W("method"), NULL, &result, numparams, params, obj);

// プロパティ取得
obj->PropGet(0, TJS_W("prop"), NULL, &var, obj);

// プロパティ設定
obj->PropSet(TJS_MEMBERENSURE, TJS_W("prop"), NULL, &var, obj);

// インデックスアクセス
obj->PropGetByNum(0, index, &var, obj);
obj->PropSetByNum(TJS_MEMBERENSURE, index, &var, obj);

// メンバ削除
obj->DeleteMember(0, TJS_W("name"), NULL, obj);

// 参照カウント
obj->AddRef();
obj->Release();
```

### エラーコード

| 定数 | 値 | 意味 |
|---|---|---|
| `TJS_S_OK` | 0 | 成功 |
| `TJS_S_TRUE` | 1 | 真 |
| `TJS_S_FALSE` | 2 | 偽 |
| `TJS_E_MEMBERNOTFOUND` | -1001 | メンバが見つからない |
| `TJS_E_NOTIMPL` | -1002 | 未実装 |
| `TJS_E_INVALIDPARAM` | -1003 | 無効なパラメータ |
| `TJS_E_BADPARAMCOUNT` | -1004 | パラメータ数が不正 |
| `TJS_E_INVALIDTYPE` | -1005 | 無効な型 |
| `TJS_E_NATIVECLASSCRASH` | -1008 | ネイティブクラスエラー |

チェックマクロ: `TJS_SUCCEEDED(r)`, `TJS_FAILED(r)`

### よく使うユーティリティ関数

```cpp
TVPGetScriptDispatch()       // TJS2 グローバルオブジェクト取得
TVPExecuteScript(script, &result)  // TJS2 スクリプト実行
TVPExecuteExpression(expr, &result) // TJS2 式評価
TVPAddLog(ttstr)             // ログ出力
TVPAddImportantLog(ttstr)    // 重要ログ出力
TVPThrowExceptionMessage(msg) // TJS2 例外送出
TJSCreateArrayObject()       // 空の Array 生成
TJSCreateDictionaryObject()  // 空の Dictionary 生成
TJS_W("string")              // ワイド文字列リテラル
```

---

## 19. ビルド設定

### DLL プラグイン

`V2Link` と `V2Unlink` をエクスポートする必要があります。
`ncbind.cpp` がこれらの定義とエクスポート設定を含んでいるので、
通常はリンクするだけで済みます。

**MSVC:**
`ncbind.cpp` 内の `#pragma comment(linker, "/EXPORT:V2Link")` により自動設定されます。

**gcc / MinGW:**
`ncbind.def` を使用するか、`ncbind.cpp` 内の asm ディレクティブにより自動設定されます。

### 静的プラグイン (TVP_STATIC_PLUGIN)

DLL ではなく本体に直接リンクする場合:

```
-DTVP_STATIC_PLUGIN -DTVP_PLUGIN_NAME=myplugin
```

`ncbind.cpp` が `krkrz_plugin_myplugin` というエントリ関数を自動生成します。

### インクルードパス

```
-I../tp_stub   (tp_stub.h のあるディレクトリ)
```

### CMake

`tp_stub/krkrz.cmake` を参照してください。

---

## 20. 制限事項と注意点

### 継承非サポート
登録したクラス間の継承関係は認識されません。
`instanceof` で派生クラスのチェックはできず、
引数に派生クラスインスタンスを渡すとエラーになります。

### コンストラクタは1つだけ
1つのクラスに複数のコンストラクタを登録することはできません。
可変引数が必要な場合は Factory パターンを使ってください。

### デフォルト引数非サポート
引数の省略はサポートされていません。TJS2 から渡す引数の個数は
メソッドの引数の個数と一致する必要があります。
可変長引数が必要な場合は RawCallback を使ってください。

### 返り値としてインスタンスを返す際の注意

| 方法 | 動作 | 注意 |
|---|---|---|
| 値で返す | コピーコンストラクタで新規生成 | — |
| 参照で返す | そのまま TJS2 へ (delete されない) | ライフタイム管理に注意 |
| ポインタで返す | そのまま TJS2 へ (invalidate 時に delete) | 二重 delete の危険あり |

### その他
- 参照で値を書き換えて返すメソッドは対応不可 → RawCallback で対処
- 同じクラスの多重登録はエラー
- const 返り値の参照/ポインタは const が解除される
