// ================================================================================
// GUISampleForVS
// アプリケーションのエントリ ポイントを定義します。
// ================================================================================


#include "stdafx.h"
#include "GUISampleForVS.h"
#include "GUIObject.h"


#define MAX_LOADSTRING 100

// グローバル変数:
HINSTANCE g_hInst;								// 現在のインターフェイス
HBITMAP   g_hBitmap = NULL;
POINTS    g_pos = { 0, 0 };
TCHAR szTitle[MAX_LOADSTRING];					// タイトル バーのテキスト
TCHAR szWindowClass[MAX_LOADSTRING];			// メイン ウィンドウ クラス名
bool g_bInPaint = false;						// 描画中か？
// ダブルバッファリングでちらつき対策.
HBITMAP hDBuf = NULL;
HDC hDDc = NULL;
HPEN hPen;

LRESULT CALLBACK MyDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp);

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	{
		// TODO: ここに初期化に関係するコードを挿入してください。
		srand(GetRandomSeed());
	}

	//MSG msg;

	// グローバル文字列を初期化しています。
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_GUISAMPLEFORVS, szWindowClass, MAX_LOADSTRING);

	g_hInst = hInstance;
	DialogBox(hInstance, (LPCTSTR)IDD_DIALOG1, NULL, (DLGPROC)MyDlgProc);

	return 0;
}

// 描画開始.
// こちらは変更の必要はないはず.
void BeginRender(HWND hDlgWnd, HDC& hdc)
{
	
	// バッファがあれば削除.
	if (hDBuf) {
		DeleteDC(hDDc);
		DeleteObject(hDBuf);
		hDBuf = NULL;
	}

	// ダブルバッファ作成.
	hdc = GetDC(hDlgWnd);
	hDDc = CreateCompatibleDC(hdc);					// 仮想デバイスコンテキスト作成.
	hDBuf = CreateCompatibleBitmap(hdc, 800, 800);	// 仮想デバイスコンテキスト用BMP作成.
	SelectObject(hDDc, hDBuf);
	ReleaseDC(hDlgWnd, hdc);
	hdc = hDDc;

	SelectObject(hdc, hPen);

	// 背景塗りつぶし
	{
		RECT rect;
		::GetClientRect(hDlgWnd, &rect);
		HBRUSH hBrush = CreateSolidBrush(RGB(250, 250, 250));
		FillRect(hdc, &rect, hBrush);

		DeleteObject(hBrush);
	}
}

// 描画終了.
// こちらは変更の必要はないはず.
void EndRender(HWND hDlgWnd)
{
	// ダブルバッファを描画.
	if (hDDc) {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hDlgWnd, &ps);
		BitBlt(hdc, 0, 0, 800, 800, hDDc, 0, 0, SRCCOPY);
		EndPaint(hDlgWnd, &ps);
	}
}


LRESULT CALLBACK MyDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const UINT unMY_TIMER = 12345;
	static BOOL s_bStart = false;

	// Windowの出現位置を固定するには下記のコードを有効にしてください
	//SetWindowPos(hDlgWnd, NULL, 0, 0, 800, 800, SWP_SHOWWINDOW);

	static UINT s_unTimer = 0;
	
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			g_hBitmap = ::LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP1));
			hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
			ResetObject();

			// スタイル追加(ちらつき防止).
			LONG_PTR style = GetWindowLongPtr(hDlgWnd, GWL_STYLE);
			style |= WS_CLIPCHILDREN;
			SetWindowLongPtr(hDlgWnd, GWL_STYLE, style);
		}
		return TRUE;
	case WM_CLOSE:
		EndDialog(hDlgWnd, IDCLOSE);
		break;
	case WM_TIMER:
		InvalidateRect(hDlgWnd, NULL, FALSE);  // 再描画命令.
		break;
	case WM_COMMAND:
		switch (LOWORD(wp))
		{
		case IDOK:
			EndDialog(hDlgWnd, IDOK);
			break;
		case IDSOLVE:
			// コリジョン解決の処理を書く.
			if (!s_bStart) {
				SetTimer(hDlgWnd, unMY_TIMER, GetUpdateMsec(), NULL);
				s_bStart = !s_bStart;
			}
			else {
				KillTimer(hDlgWnd, unMY_TIMER);
				s_bStart = !s_bStart;
			}
			break;
		case IDRESET:
			// オブジェクトの配置をリセットする処理を書く.
			if (s_bStart) {
				KillTimer(hDlgWnd, unMY_TIMER);
				s_bStart = !s_bStart;
			}
			ResetObject();
			
			s_unTimer = 0;
			InvalidateRect(hDlgWnd, NULL, FALSE);  // 再描画命令.
			break;
		default:
			return FALSE;
		}
		break;
	case WM_PAINT:
		// 描画イベント.
		HDC hdc;
		{
			g_bInPaint = true;
			// 描画開始.
			BeginRender(hDlgWnd, hdc);
			
			// オブジェクト更新.
			UpdateObject(hdc, s_unTimer);
			s_unTimer++;
			
			// コップ描画.
			RenderCup(hdc);

			// 描画終了.
			EndRender(hDlgWnd);
			g_bInPaint = false;
		}
		break;
	case WM_LBUTTONDOWN:  // 左クリックイベント
		g_pos = MAKEPOINTS(lp);
		{
			// TODO: 左クリック時のコードをここに追加してください.
		}

		InvalidateRect(hDlgWnd, NULL, FALSE);  // 再描画命令.
		break;
	case WM_DESTROY:
		DeleteObject(hPen);
		break;
	default:
		return FALSE;
	}
	return FALSE;
}







