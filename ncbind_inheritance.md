# ncbind 単一継承サポート — 調査記録と設計メモ

ncbind は本来フラットなバインダで、登録クラス間の継承関係を持たない
（`ncbind_manual.md` §20 制限事項 / `readme.md` 参照）。
本書で述べる単一継承サポートの利用者向け解説は `ncbind_manual.md` §21 にある。
本書は「C++ 層で単一継承バインダ（他言語の Sqrat 相当）を TJS2 上に構築できるか」を
エンジン仕様に基づいて調査した記録と、その実装設計をまとめたもの。

結論: **TJS2/NativeInstance の仕様だけで単一継承バインダは構築可能**であり、
むしろ Sqrat が土台の Squirrel C-API に委譲していた継承機構を、TJS2 は最初から備えている。

---

## 1. 調査結果 — TJS2 側の継承基盤

参照は吉里吉里Z 本体 `src/core/common/tjs2/` 配下。

### 1.1 instanceof はクラス名の配列走査

- `tTJSCustomObject` は per-instance で `std::vector<ttstr> ClassNames` を持つ
  （`tjsObject.h:494`）。
- `IsInstanceOf(membername=NULL)` はこの配列を線形全走査し、一致すれば真を返す
  （`tjsObject.cpp:1886-1929`、特に `1902-1908`）。`"Object"` は常に真。
- クラス名の追加口は `ClassInstanceInfo(TJS_CII_ADD)` ただ一つ
  （`tjsObject.cpp:2100-2146`、push は `2114`）。
  登録順は「子孫 → 祖先」で、先頭がそのオブジェクトの主クラス名
  （`tjsObject.cpp:2111-2113` のコメント）。
- `instanceof` 演算子（`VM_CHKINS`）は最終的にこの走査に帰着する
  （`tjsInterCodeExec.cpp:3018-3041`）。

→ **祖先クラス名を CII_ADD で積みさえすれば `instanceof 基底` が真になる。**
  エンジン内実例: `NullDrawDevice.cpp:79-81` が互換のため自クラス名に加えて
  `"BasicDrawDevice"` を手動 CII_ADD している。

### 1.2 1オブジェクトが複数のネイティブインスタンスを保持できる

- ネイティブインスタンスは並列 vector `ClassIDs` / `ClassInstances` で保持
  （`tjsObject.h:479-480`、実装 `tjsObject.cpp:2072-2098`）。
- `NativeInstanceSupport(TJS_NIS_REGISTER)` は**重複チェックなし**で
  `(classid, instance)` を push（`2090-2095`）。GET は classid 一致で線形取得（`2076-2088`）。
- native class id は `TJSRegisterNativeClass` がグローバル配列添字として採番
  （`tjsNative.cpp:24-52`）。

→ **1つの TJS オブジェクトに、基底 classid と派生 classid の
  複数ネイティブインスタンスを同時に登録できる。** これがネイティブ継承の下地。

### 1.3 ネイティブ継承の実体は `FuncCall(membername=NULL)`

エンジンのネイティブクラス構築の中核は `tTJSNativeClass::FuncCall(membername=NULL)`
（`tjsNative.cpp:342-412`）。1回の呼び出しで次を**一括**で行う:

1. `ClassInstanceInfo(TJS_CII_ADD, ClassName)` — クラス名を objthis に追加（`360-361`）
2. `CreateNativeInstance()` → `NativeInstanceSupport(TJS_NIS_REGISTER, _ClassID)`
   — 空でも native instance を登録（`364-368`）
3. `EnumMembers` でクラスオブジェクトの全メンバを objthis へ流し込み（`372-409`）

`new` の実体 `CreateNew`（`tjsNative.cpp:414-468`）はこれを2段で使う:

- `FuncCall(0, NULL, 0 params, dsp)` — 上記のメンバ流し込み＋クラス名追加＋instance登録（`439`）
- `FuncCall(0, ClassName, 実引数, dsp)` — コンストラクタ NCM 呼び出し（`450`）

そしてスクリプトの `class Derived extends Base` は、コンパイル時に
「基底クラス本体を同じ objthis に対し `membername=NULL` で呼ぶ」コードを生成する
（`tjsInterCodeGen.cpp:3494-3527` の `VM_CHGTHIS`＋`VM_CALL`）。
基底がネイティブなら上記 `FuncCall(NULL)` が走り、基底名の CII_ADD・基底 instance 登録・
基底メンバ流し込みが自動で起きる。

→ **「コンストラクタの多重呼び」= 基底クラスオブジェクトの `FuncCall(NULL)` を
  同じ objthis に対して呼ぶこと**であり、それがメソッド継承・instanceof・
  native instance 継承のすべてを一度に成立させる。

### 1.4 参考: もう一系統（委譲チェーン、休眠中）

`tTJSExtendableObject`（`tjsObjectExtendable.*`）は `iTJSDispatch2* SuperClass` を1本持ち、
自分に無いメンバを SuperClass へフォールバック委譲する**単一継承専用**機構
（ヘッダに「多重継承は非対応」と明記）。`SetSuper`/`ExtendsClass`/
`ClassInstanceInfo(TJS_CII_SET_SUPRECLASS)` で設定。`tTJSNativeClass` はこれを継承しており
（`tjsNative.h:185-187`）、`CreateNew` は SuperClass 有り時に基底インスタンスを別生成して
`CII_SET_SUPRECLASS` でリンクする（`tjsNative.cpp:414-468`）。
ただし **エンジンの実クラスはどこも SuperClass を設定しておらず**、この経路は実行実績がない。
Sqrat の `_base` delegate に最も近いのはこれだが、枯れていない点に注意。

---

## 2. Sqrat（Squirrel）との対応

他言語の Sqrat は `DerivedClass<C, B>` で単一継承を表現するが、継承ロジックの本体は
Squirrel の C-API（`sq_newclass(base)`）に委譲している。対応表:

| Sqrat が Squirrel に委譲していた処理 | TJS2 での対応物 |
|---|---|
| `sq_newclass(base)` で継承リンク登録 | `FuncCall(membername=NULL)` による基底本体の連鎖呼び（= `extends` の実体） |
| 基底メンバ表を派生へ**コピー**（`SQClass::Create`） | `FuncCall(NULL)` の `EnumMembers` 流し込み／またはクラスオブジェクト間コピー |
| `_base` チェーン walk で `instanceof` | `ClassNames` 配列の線形走査（名前ベースだが等価、多段も蓄積済み） |
| 単一 `_userpointer` を基底/派生で共有 | classid ごとに独立した native instance を保持（TJS の方が強力） |

**TJS2 が優れている点**: Sqrat は単一 `_userpointer` を生ビットで reinterpret するため
「基底が常にオフセット0」の単一継承でしか正しくない。TJS2 は基底 classid ごとに
別の native instance を登録できるので、`static_cast<Base*>(derivedPtr)` で
**C++ の正しいポインタ調整**を通したうえで基底側に登録でき、オフセットずれが原理的に起きない。

**コンストラクタ連鎖**: Sqrat 同様、C++ 言語側の `new Derived()` が基底 ctor を連鎖する前提に乗る。
TJS レベルで基底 ctor を再実行する必要はなく、TJS 側は「基底クラス名の追加」と
「基底 classid への（アップキャストした）実体登録」という**簿記**だけを行う。

---

## 3. 設計 — ncbind への単一継承拡張

ゴール: **1つの C++ `Derived`（`Derived : public Base`）オブジェクトが、
TJS からは Base のインスタンスでもあるように振る舞う**（Sqrat 忠実 = 単一実体共有）。

利用者が期待する具体的な振る舞いは次の3点:

1. **記述量最小** — C++ 側で既に継承済みの実体を登録する際、派生クラスは
   「追加したメンバだけ」を登録記述すれば、基底クラスの定義（メソッド／プロパティ／定数）を
   **全部自動で引き継ぐ**。現状のように基底メンバを個別にコピペ／マクロで再列挙する必要をなくす。
2. **InstanceOf 自動** — `d instanceof "Base"` も `d instanceof "Derived"` も自動的に真になる
   （派生・全祖先のクラス名を自動 CII_ADD）。
3. **型に応じた実体取り出し** — `tTJSVariant`（TJS オブジェクト）から C++ 実体を引き出す際、
   引数・返り値が `Derived*` を要求すれば派生ポインタ、`Base*` を要求すれば
   **正しくオフセット調整された基底ポインタ**が返る。同一 C++ 実体を、派生 classid には
   `Derived*`（所有）、基底 classid には `static_cast<Base*>(d)`（sticky・非所有）で
   二重登録することで実現する。

### 3.1 採用案 B-2（連鎖＋ポインタ共有）

新しい登録形（仮）:

```cpp
// Base は通常どおり
NCB_REGISTER_CLASS(Base) {
    Constructor();
    NCB_METHOD(baseMethod);
    NCB_PROPERTY(baseProp, getBaseProp, setBaseProp);
}

// Derived : public Base を Base の派生として登録
NCB_REGISTER_SUBCLASS_OF(Derived, Base) {   // ← 追加するマクロ
    Constructor();
    NCB_METHOD(derivedMethod);
}
```

TJS からは:

```tjs
var d = new Derived();
d.derivedMethod();               // 派生メソッド
d.baseMethod();                  // 継承した基底メソッド（同一 C++ 実体を操作）
if (d instanceof "Base") { ... } // 真
if (d instanceof "Derived") { ... } // 真
```

### 3.2 2フェーズ構成

**(A) メンバ継承（登録後の一括処理）**

全クラス登録が済んだ後（post-regist パス）に、
基底クラスオブジェクトのメンバを派生クラスオブジェクトへコピーする。

- コピーは基底クラスオブジェクトを `EnumMembers` し、各メンバを派生クラスオブジェクトへ
  `PropSet(TJS_MEMBERENSURE|TJS_IGNOREPROP|flags)`（エンジンの流し込みと同手法、
  `tjsNative.cpp:394-397` を踏襲）。
- **コピー順が override 順を決める**: 基底を先に入れ、派生自身の登録が上書きする形にする。
  そのため「基底 → 派生」の順でコピーを流す（派生自身のメンバは既に派生クラスオブジェクトに
  登録済みなので、コピー時は `TJS_MEMBERENSURE` でも派生を消さないよう、
  基底メンバのうち派生に既存の名前はスキップする）。
- 基底のコンストラクタ NCM（名前 == 基底クラス名）と `finalize` はスキップ。
- コピーされた基底メソッド NCM は `ncbInstanceAdaptor<Base>::GetNativeInstance` で
  Base classid の実体を引くため、(B) の登録が前提になる。

タイミングに post-regist を使うのは、登録順（基底が派生より後に登録され得る）に
依存しないため。first-construct で行うと、そのクラスオブジェクトのメンバ流し込みは
`CreateNew` の `FuncCall(NULL)`（`tjsNative.cpp:439`）で既に済んでいて
初回インスタンスに基底メンバが乗らないので不可。

**(B) 実体共有＋クラス名追加（construct 時）**

派生コンストラクタ（`ncbNativeClassConstructor`／Factory）で `Derived* d` を生成した後:

1. `SetNativeInstance<Derived>(objthis, d)` — 派生 classid に所有アダプタとして登録（既存）。
2. 各祖先 `Ancestor` について:
   - `objthis->ClassInstanceInfo(TJS_CII_ADD, 0, &ancestorName)` — instanceof 用。
   - `ncbInstanceAdaptor<Ancestor>` を生成し `_instance = static_cast<Ancestor*>(d)` を設定、
     **sticky（非所有）** で `NativeInstanceSupport(TJS_NIS_REGISTER, ancestorClassID)`。
     → 基底メソッドはこの共有アップキャスト実体を触る。二重 delete は sticky で回避。

`static_cast<Ancestor*>(d)` を C++ で通すので、多重継承や非先頭サブオブジェクトでも
ポインタは正しい（Sqrat の弱点を回避）。

### 3.3 多段継承

`Derived → Mid → Base` は、各サブクラス登録形が自分の直近の基底型を
コンパイル時トレイト（例: `ncbSubClassOf<T>::BaseT`、未継承なら `void`）で公開し、
(A)(B) を基底チェーンに沿って再帰適用する:

- (A) メンバコピー: `Base → Mid → Derived` の順に流せば override 順が保たれる
  （祖先ほど先）。
- (B) construct: `Mid` と `Base` の両方について CII_ADD＋アップキャスト共有登録。
  `ClassNames` は `[Derived, Mid, Base]` の子孫→祖先順になる。

初期実装（PoC）は**単一段**を対象とし、多段は上記トレイト再帰で拡張する。

### 3.4 代替案（記録）

- **B-1 純粋連鎖（別実体）**: 派生 construct で基底の ncbind コンストラクタを普通に呼ぶと、
  基底が**別の** `new Base()` として生成される。TJS ネイティブ `extends` と同じ意味論で
  instanceof・メソッド継承は成立するが、`Derived is-a Base` の状態を1個の C++ 実体で
  共有できない。C++ 継承バインダとしては不適だが、「派生 TJS クラスに独立した
  基底ロジックを合成したい」用途なら選べる。
- **案A 手動メンバ複製**: 派生登録ブロックで基底のメンバ登録を手書きで再列挙する
  （本体プラグイン `win32ole` の `ActiveX : public WIN32OLE` が実際に採る手法）。
  自動継承にはならないが、依存が少なく確実。
- **委譲チェーン（`tTJSExtendableObject`/`SetSuper`）**: §1.4。単一継承専用で Sqrat の
  `_base` に最も近いが、エンジン実行実績がなく採用は保留。

### 3.5 制約・注意

- **単一継承のみ**（Squirrel/Sqrat と同じ制約。TJS 側は多段は可、多重は非対応）。
- メンバは「コピー（複製）」であり委譲ではないので、登録後に基底クラスへ
  メソッドを足しても既存派生クラスには波及しない（Squirrel と同じ挙動）。
- クラス名の CII_ADD は重複チェックがないため、同名クラスを二重に積まないよう
  バインダ側で管理する。
- 所有権: 実体を所有するアダプタは派生 classid のもの1つだけ。基底 classid 側は
  必ず sticky（非所有）にして二重 delete を防ぐ。

---

### 3.6 実装状況（2026-08-01 実装・検証済）

本設計は `ncbind.hpp` に実装済み。公開マクロは
**`NCB_REGISTER_SUBCLASS_OF(cls, base)`** / `NCB_REGISTER_SUBCLASS_OF_DIFFER(name, cls, base)`。
主な追加物:

- トレイト `ncbSubClassOf<T>`（直近基底型。未継承は `void`）。
- construct 時の祖先 attach: `ncbAncestorAttacher<T>` / `ncbSubClassAttachAncestors<>`、
  `ncbInstanceAdaptor<>::SetSharedNativeInstance`（アップキャスト実体を sticky 登録）。
- 継承対応 NCM: `ncbNativeSubClassConstructor` / `ncbNativeSubClassFactory`、
  proxy の `Constructor`/`Factory` が `ncbCtorSelect`/`ncbFactorySelect` で自動選択。
- PostRegist メンバコピー: `ncbSubClassMemberCopy<>`（マクロが post-regist callback を emit）。

検証: 検証用プラグイン `src/plugins/ncbindtest/`（`main.cpp` + `startup.tjs`）で
WINVER/SDL 両ビルド + WINVER 実機 round-trip、**全23項目 pass**。

**実装上の落とし穴（将来の保守用に記録）**:

1. **メンバコピー時に closure の objthis を束ね直さないこと。**
   クラスオブジェクト上のメンバは objthis=null で保持され、インスタンス生成時の
   メンバ流し込み（`tTJSNativeClass::FuncCall`, `tjsNative.cpp:390`）で初めて
   インスタンスへ束縛される。コピー時に `ChangeClosureObjThis(派生クラスオブジェクト)` で
   null を潰すと、その後インスタンスへ再束縛されず、呼び出し時に
   「実行コンテキストが違います」になる。コピーは値をそのまま（null 保持で）PropSet する。
2. **多段のメンバコピーは「コピー先を派生クラスオブジェクトに固定」して祖先を辿ること。**
   再帰で「基底のさらに基底」を基底クラスオブジェクトへコピーしても派生には届かない。
   `CopyInto(dst)` で dst を固定し、全祖先の own メンバを同じ dst へ流す（近い祖先から
   copy + skip-existing で override 順も維持）。

### 3.7 追補（2026-08-01）: CreateAdaptor 経路でも祖先 attach する

当初の祖先 attach は ctor / factory NCM（＝TJS の `new`）でしか走らなかった。
しかし **コンバータ復路など `iTJSDispatch2` を直接生成する経路**（プラグインが
C++ 側で生成したネイティブ実体を `ncbInstanceAdaptor<T>::CreateAdaptor()` で TJS
オブジェクト化して返すケース。例: threepp の loader 戻り値・シーングラフ traversal）
は `new` を通らないため、基底 classid の共有登録も基底名の CII_ADD も行われず、
「その TJS オブジェクトを基底型引数へ渡すと実体が取り出せない / `instanceof 基底`
が偽」という穴があった。

対策として `CreateAdaptor()` が `_instance` を設定した直後に
`ncbSubClassAttachAncestors<NativeClassT>(obj)` を呼ぶよう変更（前方宣言を
`ncbSubClassOf` 付近に追加）。非継承クラスは `BaseT=void` で no-op なので無条件でよい。
sticky 生成時（既にアップキャスト共有登録された実体を包む場合）は再 attach しないよう
`!sticky` でガードする。

### 3.8 応用例: shared_ptr ラッパ経由での is-a（krkrthreepp）

`ThreeWrapper<T>`（`shared_ptr<T>` を保持するラッパを classid にするパターン）でも、
**ラッパ自身を threepp の階層に沿って C++ 継承させれば** is-a を適用できる:

- `ThreeWrapper<Derived> : public ThreeWrapper<Base>`（トレイト `ThreeBaseOf<T>` で基底を宣言）。
  各レベルが自分の型の `shared_ptr` を持ち、`setObject` で基底へ `static_pointer_cast`
  伝播する（基底 classid の Bridge が正しい raw ポインタを返せるように）。
- `ncbSubClassOf<ThreeWrapper<T>>` を `ThreeBaseOf<T>` から自動導出する部分特殊化を1つ置く。
- boxing 登録パス（`NCB_REGISTER_SUBCLASS`）でも ctor/factory select は共通ビルダ経由で
  効くため、PostRegist メンバコピーを emit すれば member 継承も成立する。
- 祖先 attach の穴（§3.7）はこのパターンで顕在化する（loader 戻り値が復路生成のため）。
  §3.7 の CreateAdaptor 対応で解消。

## 4. 検証項目（実装後）

krkrz 実機（`testbind` 相当）で round-trip 検証する:

- `new Derived()` 後の `instanceof "Base"` / `instanceof "Derived"` がともに真。
- 継承した基底メソッド・プロパティが**派生実体の状態**を正しく読み書きする
  （基底 this と派生 this が同一オブジェクトを指すこと）。
- 派生でのメソッド override が効く（基底同名メソッドを上書き）。
- `invalidate` 時に二重 delete しない。
- 多段継承（3段）で全祖先の instanceof とメソッドが通る。

---

## 5. 参考（本体プラグインの既存継承手法）

C++ 層の本拡張が入るまでの間、既存プラグインは次の回避策で is-a を表現している
（詳細な手法比較は本体プラグイン群のソース参照）:

- `win32ole` — C++ `ActiveX : public WIN32OLE`＋派生登録で基底コールバックを明示再登録。
- `win32dialog` — ネイティブ基底＋TJS 多段 `extends`（`super.基底()` で実体共有）。
- `krkr_richtext` — `NCB_REGISTER_CLASS_DIFFER` で基底別名を作り TJS `extends`。
- `windowEx` 系 — `NCB_ATTACH_CLASS_WITH_HOOK` でエンジン組込みクラス（Layer/Window 等）に
  ネイティブ機能を付加し、TJS `extends 組込みクラス` に見せる。

なお `NCB_REGISTER_SUBCLASS`（`ncbind_manual.md` §14）は名前空間ネスト（has-a）であり、
本書の is-a 継承とは無関係。命名の混同に注意。
