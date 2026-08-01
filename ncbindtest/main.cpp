// ncbind is-a 単一継承 (NCB_REGISTER_SUBCLASS_OF) の検証用プラグイン
//
// C++ で既に継承関係にあるクラス群を登録し、
//   1. 派生は「追加メンバのみ」記述で基底定義を全継承する
//   2. instanceof が派生・全祖先で真になる
//   3. tTJSVariant からの実体取り出しが型(派生/基底)に応じて正しいポインタを返す
// を TJS 実機で round-trip 検証する。
//
// 詳細設計: src/plugins/ncbind/ncbind_inheritance.md

#include <cstdint>
#include "ncbind.hpp"

//----------------------------------------------------------------------
// C++ 側は普通の単一継承。バインダ登録以外に特別な記述は不要。

/// 基底
class TestBase {
protected:
	int _base;
public:
	TestBase() : _base(0) {}
	virtual ~TestBase() {}

	void  setBase(int v) { _base = v; }
	int   getBase() const { return _base; }
	ttstr who()          { return ttstr(TJS_W("TestBase")); } //< 派生で override される

	// この this が指す TestBase サブオブジェクトのアドレス
	tjs_int64 baseAddr() { return (tjs_int64)(intptr_t)this; }
};

/// 派生 (追加メンバは setDerived/getDerived と who の override のみ)
class TestDerived : public TestBase {
	int _derived;
public:
	TestDerived() : _derived(0) {}

	void  setDerived(int v) { _derived = v; }
	int   getDerived() const { return _derived; }
	ttstr who()             { return ttstr(TJS_W("TestDerived")); } //< override
};

/// 多段 (派生の派生)
class TestGrand : public TestDerived {
	int _grand;
public:
	TestGrand() : _grand(0) {}

	void setGrand(int v) { _grand = v; }
	int  getGrand() const { return _grand; }
};

/// オフセット検証用: 先頭に別基底を挟むことで TestBase サブオブジェクトを
/// オフセット 0 以外に置く。static_cast アップキャストが効いているかを見る。
class Prefix {
	tjs_int64 _pad;
public:
	Prefix() : _pad(0x1234) {}
	virtual ~Prefix() {}
};
class TestOffChild : public Prefix, public TestBase {
public:
	ttstr childTag() { return ttstr(TJS_W("TestOffChild")); }
};

//----------------------------------------------------------------------
// 型別ポインタ取り出しの検証用グローバル関数。
// ncbind は引数型の classid でネイティブ実体を引くので、
// 同一オブジェクトを渡しても「要求した型」のポインタが返るはず。

static tjs_int64 ncbtestAsBasePtr    (TestBase     *p) { return (tjs_int64)(intptr_t)p; }
static tjs_int64 ncbtestAsDerivedPtr (TestDerived  *p) { return (tjs_int64)(intptr_t)p; }
static tjs_int64 ncbtestAsOffBasePtr (TestBase     *p) { return (tjs_int64)(intptr_t)p; }
static tjs_int64 ncbtestAsOffChildPtr(TestOffChild *p) { return (tjs_int64)(intptr_t)p; }

//----------------------------------------------------------------------
// 登録: 派生は「追加分だけ」列挙する。

NCB_REGISTER_CLASS(TestBase) {
	Constructor();
	NCB_METHOD(setBase);
	NCB_METHOD(getBase);
	NCB_METHOD(who);
	NCB_METHOD(baseAddr);
	NCB_PROPERTY_RO(baseValue, getBase);
}

NCB_REGISTER_SUBCLASS_OF(TestDerived, TestBase) {
	Constructor();
	NCB_METHOD(setDerived);
	NCB_METHOD(getDerived);
	NCB_METHOD(who);           //< 基底の who を override (member-copy はこれを尊重)
}

NCB_REGISTER_SUBCLASS_OF(TestGrand, TestDerived) {
	Constructor();
	NCB_METHOD(setGrand);
	NCB_METHOD(getGrand);
}

NCB_REGISTER_SUBCLASS_OF(TestOffChild, TestBase) {
	Constructor();
	NCB_METHOD(childTag);
}

NCB_REGISTER_FUNCTION(ncbtestAsBasePtr,     ncbtestAsBasePtr);
NCB_REGISTER_FUNCTION(ncbtestAsDerivedPtr,  ncbtestAsDerivedPtr);
NCB_REGISTER_FUNCTION(ncbtestAsOffBasePtr,  ncbtestAsOffBasePtr);
NCB_REGISTER_FUNCTION(ncbtestAsOffChildPtr, ncbtestAsOffChildPtr);
