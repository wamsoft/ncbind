#include "ncbind.hpp"

#ifdef TVP_STATIC_PLUGIN

#define EXPORT(hr) static hr STDCALL

#else

#if defined(_MSC_VER)
    #define DLL_EXPORT  __declspec(dllexport)
#else
	#define DLL_EXPORT  __attribute__((visibility("default")))
#endif

#define EXPORT(hr) extern "C" DLL_EXPORT hr STDCALL

#ifdef _WIN32

#ifdef _MSC_VER
# if defined(_M_AMD64) || defined(_M_X64)
#  pragma comment(linker, "/EXPORT:V2Link")
#  pragma comment(linker, "/EXPORT:V2Unlink")
# else
#  pragma comment(linker, "/EXPORT:V2Link=_V2Link@4")
#  pragma comment(linker, "/EXPORT:V2Unlink=_V2Unlink@0")
# endif
#endif
#ifdef __GNUC__
asm (".section .drectve");
# if defined(__x86_64__) || defined(__x86_64)
asm (".ascii \" -export:V2Link=V2Link -export:V2Unlink=V2Unlink\"");
# else
asm (".ascii \" -export:V2Link=V2Link@4 -export:V2Unlink=V2Unlink@0\"");
# endif
#endif

//--------------------------------------
HINSTANCE gDllInstance = NULL;

//--------------------------------------
extern "C"
BOOL WINAPI
DllMain(HINSTANCE hinst, DWORD reason, LPVOID /*lpReserved*/)
{
  if (reason == DLL_PROCESS_ATTACH)
    gDllInstance = hinst;

  return 1;
}
#endif

#endif // TVP_STATIC_PLUGIN

//---------------------------------------------------------------------------
static tjs_int GlobalRefCountAtInit = 0;

EXPORT(HRESULT) V2Link(iTVPFunctionExporter *exporter)
{
	// スタブの初期化(必ず記述する)
	TVPInitImportStub(exporter);

	NCB_LOG_W("V2Link");

	// AutoRegisterで登録されたクラス等を登録する
	ncbAutoRegister::AllRegist();

	// この時点での TVPPluginGlobalRefCount の値を
	GlobalRefCountAtInit = TVPPluginGlobalRefCount;
	// として控えておく。TVPPluginGlobalRefCount はこのプラグイン内で
	// 管理されている tTJSDispatch 派生オブジェクトの参照カウンタの総計で、
	// 解放時にはこれと同じか、これよりも少なくなってないとならない。
	// そうなってなければ、どこか別のところで関数などが参照されていて、
	// プラグインは解放できないと言うことになる。

	return S_OK;
}
//---------------------------------------------------------------------------
EXPORT(HRESULT) V2Unlink()
{
	// 吉里吉里側から、プラグインを解放しようとするときに呼ばれる関数

	// もし何らかの条件でプラグインを解放できない場合は
	// この時点で E_FAIL を返すようにする。
	// ここでは、TVPPluginGlobalRefCount が GlobalRefCountAtInit よりも
	// 大きくなっていれば失敗ということにする。
	if (TVPPluginGlobalRefCount > GlobalRefCountAtInit) {
		NCB_LOG_W("V2Unlink ...failed");
		return E_FAIL;
		// E_FAIL が帰ると、Plugins.unlink メソッドは偽を返す
	} else {
		NCB_LOG_W("V2Unlink");
	}
	/*
		ただし、クラスの場合、厳密に「オブジェクトが使用中である」ということを
		知るすべがありません。基本的には、Plugins.unlink によるプラグインの解放は
		危険であると考えてください (いったん Plugins.link でリンクしたら、最後ま
		でプラグインを解放せず、プログラム終了と同時に自動的に解放させるのが吉)。
	*/

	// AutoRegisterで登録されたクラス等を削除する
	ncbAutoRegister::AllUnregist();

	// スタブの使用終了(必ず記述する)
	TVPUninitImportStub();

	return S_OK;
}

#ifdef TVP_STATIC_PLUGIN

#if defined(_MSC_VER)
    #define EXPORT_USED __declspec(dllexport)
#else
	#define EXPORT_USED __attribute__((visibility("default"), used))
#endif

#define str(x) TJS_W(#x)
#define strx(x) str(x)
#define CAT(a, b) a##b
#define XCAT(a, b) CAT(a, b)
#define MAKE_FUNC(name) XCAT(krkrz_plugin_, name)

// リンク用エントリ関数
// _krkrz_plugin_プロジェクト名 で関数が作られる
extern "C" EXPORT_USED void STDCALL MAKE_FUNC(TVP_PLUGIN_NAME)() {
	static iTVPStaticPlugin plugin;
    plugin.name = strx(TVP_PLUGIN_NAME);
	plugin.link = (int32_t (STDCALL *)(iTVPFunctionExporter *))V2Link;
	plugin.unlink = (int32_t (STDCALL *)(void))V2Unlink;
	TVPRegisterPlugin(&plugin);
}

#endif
