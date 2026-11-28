# Copyright (c) 2025
# SPDX-License-Identifier: MIT

if(CONFIG_CLEAVE_BT_PROFILE_DUMP)
  # 設定ツールのファーム更新で版を比べるため、ビルド元のコミットを埋め込む
  execute_process(
    COMMAND git -c safe.directory=* -C ${CMAKE_CURRENT_LIST_DIR} rev-parse --short=7 HEAD
    OUTPUT_VARIABLE CLEAVE_FW_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT CLEAVE_FW_SHA)
    set(CLEAVE_FW_SHA "unknown")
  endif()
  target_sources(app PRIVATE src/bt_profile_dump.c)
  target_compile_definitions(app PRIVATE CLEAVE_FW_SHA="${CLEAVE_FW_SHA}")
endif()
