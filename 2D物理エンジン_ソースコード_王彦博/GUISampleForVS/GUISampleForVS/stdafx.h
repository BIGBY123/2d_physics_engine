// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーから使用されていない部分を除外します。
// Windows ヘッダー ファイル:
#define NOMINMAX
#include <windows.h>

// C ランタイム ヘッダー ファイル
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <assert.h>


extern bool g_bInPaint;

#ifdef _DEBUG
#define PAINT_ASSERT(condition) \
	do { \
        if (!(condition)) { \
            TCHAR chBuf[256]; \
            _stprintf_s(chBuf, _T("Assertion failed: %s\nFile: %s\nLine: %d"), _T(#condition), _T(__FILE__), __LINE__); \
            TRACE(chBuf); \
			DebugBreak(); \
        } \
    } while (0)


#define ASSERT(condition) \
    do { \
        if (g_bInPaint) { \
			PAINT_ASSERT(condition); \
        } \
		else { \
			assert(condition); \
		} \
    } while (0)
#else
#define ASSERT(condition) ((void)0)
#endif

