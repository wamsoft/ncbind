# ncbind is-a 単一継承 (NCB_REGISTER_SUBCLASS_OF) 検証用プラグイン
#
# ncbind ライブラリ同梱のテスト。umbrella ビルドでは TPSTUB_DIR / NCBIND_DIR が
# トップレベルで定義済みなのでそちらが使われる。スタンドアロン configure 用に、
# ncbind 内部配置 (../../tp_stub, 親 = ncbind 本体) を既定補完する。
if(NOT DEFINED TPSTUB_DIR)
    set(TPSTUB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../tp_stub CACHE PATH "Path to tp_stub directory")
endif()
if(NOT DEFINED NCBIND_DIR)
    set(NCBIND_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. CACHE PATH "Path to ncbind library")
endif()
include(${TPSTUB_DIR}/krkrz.cmake)

krkrz_plugin(ncbindtest
    NCBIND
    SOURCES
        main.cpp
)
