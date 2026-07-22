//
// SnakeWindow.cpp
//
// Implementation of the standalone Snake game window.  Rendering is done with
// plain GDI into an off-screen bitmap (double buffering) to avoid flicker.
//

#include "SnakeWindow.h"
#include <string>
#include <cstdlib>

static const wchar_t* kClassName = L"EuroScopeSnakeWindowClass";
static const wchar_t* kWindowTitle = L"EuroScope Snake";

// Game tick interval in milliseconds (lower = faster snake).
static const UINT kBaseTickMs = 120;

SnakeWindow::SnakeWindow()
	: m_thread(NULL)
	, m_hWnd(NULL)
	, m_open(false)
	, m_length(0)
	, m_dir(DIR_RIGHT)
	, m_pendingDir(DIR_RIGHT)
	, m_score(0)
	, m_best(0)
	, m_gameOver(false)
	, m_paused(false)
	, m_timer(0)
	, m_rngState(0)
	, m_dragging(false)
	, m_hoverBtn(BTN_NONE)
{
	m_food.x = m_food.y = 0;
	m_dragOffset.x = m_dragOffset.y = 0;
}

SnakeWindow::~SnakeWindow()
{
	Close();
	if (m_thread)
	{
		// Give the thread a moment to unwind, then release the handle.
		WaitForSingleObject(m_thread, 2000);
		CloseHandle(m_thread);
		m_thread = NULL;
	}
}

bool SnakeWindow::IsOpen() const
{
	return m_open;
}

void SnakeWindow::Open()
{
	if (m_open)
	{
		// Already open: bring it to the foreground instead.
		if (m_hWnd)
		{
			ShowWindow(m_hWnd, SW_SHOW);
			SetForegroundWindow(m_hWnd);
		}
		return;
	}

	// Clean up a finished thread handle from a previous session.
	if (m_thread)
	{
		WaitForSingleObject(m_thread, 2000);
		CloseHandle(m_thread);
		m_thread = NULL;
	}

	m_open = true;
	m_thread = CreateThread(NULL, 0, &SnakeWindow::ThreadProc, this, 0, NULL);
	if (!m_thread)
		m_open = false;
}

void SnakeWindow::Close()
{
	if (!m_open)
		return;
	if (m_hWnd)
		PostMessage(m_hWnd, WM_CLOSE, 0, 0);
}

void SnakeWindow::Toggle()
{
	if (m_open)
		Close();
	else
		Open();
}

// ---------------------------------------------------------------------------
// Window thread
// ---------------------------------------------------------------------------

DWORD WINAPI SnakeWindow::ThreadProc(LPVOID param)
{
	SnakeWindow* self = reinterpret_cast<SnakeWindow*>(param);
	self->RunMessageLoop();
	self->m_open = false;
	return 0;
}

void SnakeWindow::RunMessageLoop()
{
	HINSTANCE hInst = GetModuleHandle(NULL);

	WNDCLASSEXW wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = &SnakeWindow::WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL; // we paint everything ourselves
	wc.lpszClassName = kClassName;
	// RegisterClassExW is fine to call repeatedly; ignore "already registered".
	RegisterClassExW(&wc);

	// Borderless window (WS_POPUP): no standard Windows caption/frame — we draw
	// our own EuroScope-style title bar in the client area instead.  The client
	// size IS the whole window, so no AdjustWindowRect is needed.
	// WS_EX_APPWINDOW keeps it on the taskbar; WS_MINIMIZEBOX enables minimize.
	DWORD style   = WS_POPUP | WS_MINIMIZEBOX;
	DWORD exStyle = WS_EX_APPWINDOW;

	// Centre the window on the primary monitor (CW_USEDEFAULT is unreliable for
	// WS_POPUP windows).
	const int screenW = GetSystemMetrics(SM_CXSCREEN);
	const int screenH = GetSystemMetrics(SM_CYSCREEN);
	const int posX = (screenW - CLIENT_W) / 2;
	const int posY = (screenH - CLIENT_H) / 2;

	HWND hWnd = CreateWindowExW(
		exStyle, kClassName, kWindowTitle, style,
		posX, posY,
		CLIENT_W, CLIENT_H,
		NULL, NULL, hInst, this);

	if (!hWnd)
		return;

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
	SetForegroundWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

LRESULT CALLBACK SnakeWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	SnakeWindow* self = reinterpret_cast<SnakeWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (msg)
	{
	case WM_CREATE:
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		self = reinterpret_cast<SnakeWindow*>(cs->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		self->OnCreate(hWnd);
		return 0;
	}
	case WM_KEYDOWN:
		if (self) self->OnKeyDown(wParam);
		return 0;
	case WM_MOUSEMOVE:
		if (self) self->OnMouseMove((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
		return 0;
	case WM_LBUTTONDOWN:
		if (self) self->OnLButtonDown((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
		return 0;
	case WM_LBUTTONUP:
		if (self) self->OnLButtonUp((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
		return 0;
	case WM_TIMER:
		if (self) self->OnTick();
		return 0;
	case WM_PAINT:
		if (self) self->OnPaint(hWnd);
		return 0;
	case WM_ERASEBKGND:
		return 1; // handled in WM_PAINT, prevents flicker
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		if (self) self->OnDestroy();
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Window message handlers
// ---------------------------------------------------------------------------

void SnakeWindow::OnCreate(HWND hWnd)
{
	m_hWnd = hWnd;
	m_rngState = (unsigned)GetTickCount() ^ 0x9E3779B9u;
	// m_best persists across window reopens for the plugin's lifetime.
	ResetGame();
	m_timer = SetTimer(hWnd, 1, kBaseTickMs, NULL);
}

void SnakeWindow::OnDestroy()
{
	if (m_timer)
	{
		KillTimer(m_hWnd, m_timer);
		m_timer = 0;
	}
	m_hWnd = NULL;
	m_open = false;
	PostQuitMessage(0);
}

void SnakeWindow::OnKeyDown(WPARAM key)
{
	switch (key)
	{
	case VK_UP:    case 'W': SetDirection(DIR_UP);    break;
	case VK_DOWN:  case 'S': SetDirection(DIR_DOWN);  break;
	case VK_LEFT:  case 'A': SetDirection(DIR_LEFT);  break;
	case VK_RIGHT: case 'D': SetDirection(DIR_RIGHT); break;
	case VK_SPACE:
		if (m_gameOver)
			ResetGame();
		else
			m_paused = !m_paused;
		break;
	case 'R':
		ResetGame();
		break;
	case VK_ESCAPE:
		if (m_hWnd) PostMessage(m_hWnd, WM_CLOSE, 0, 0);
		break;
	}
	if (m_hWnd) InvalidateRect(m_hWnd, NULL, FALSE);
}

SnakeWindow::TitleButton SnakeWindow::HitTestButton(int mx, int my) const
{
	if (my < 0 || my >= TITLE_H)
		return BTN_NONE;
	if (mx >= CLIENT_W - BTN_W && mx < CLIENT_W)
		return BTN_CLOSE;
	if (mx >= CLIENT_W - 2 * BTN_W && mx < CLIENT_W - BTN_W)
		return BTN_MIN;
	return BTN_NONE;
}

void SnakeWindow::OnMouseMove(int mx, int my)
{
	// While dragging the title bar, move the whole window with the cursor.
	if (m_dragging)
	{
		POINT pt;
		GetCursorPos(&pt);
		SetWindowPos(m_hWnd, NULL,
			pt.x - m_dragOffset.x, pt.y - m_dragOffset.y,
			0, 0, SWP_NOSIZE | SWP_NOZORDER);
		return;
	}

	// Track hover state over the title-bar buttons so we can highlight them.
	const TitleButton hover = HitTestButton(mx, my);
	if (hover != m_hoverBtn)
	{
		m_hoverBtn = hover;
		if (m_hWnd)
		{
			RECT bar = { 0, 0, CLIENT_W, TITLE_H };
			InvalidateRect(m_hWnd, &bar, FALSE);
		}
	}

	// Do not steer while the pointer is over the title bar.
	if (my < TITLE_H)
		return;

	// Steer the snake toward the mouse cursor: pick the dominant axis of the
	// vector from the head to the pointer.  This gives smooth mouse control
	// without needing clicks.
	if (m_gameOver || m_paused || m_length == 0)
		return;

	const int headPxX = MARGIN + m_snake[0].x * CELL + CELL / 2;
	const int headPxY = TITLE_H + HEADER + MARGIN + m_snake[0].y * CELL + CELL / 2;

	const int dx = mx - headPxX;
	const int dy = my - headPxY;

	// Small dead-zone so tiny jitter near the head doesn't cause chaos.
	if (abs(dx) < CELL / 2 && abs(dy) < CELL / 2)
		return;

	if (abs(dx) > abs(dy))
		SetDirection(dx > 0 ? DIR_RIGHT : DIR_LEFT);
	else
		SetDirection(dy > 0 ? DIR_DOWN : DIR_UP);
}

void SnakeWindow::OnLButtonDown(int mx, int my)
{
	// Title-bar buttons take priority.
	const TitleButton btn = HitTestButton(mx, my);
	if (btn == BTN_CLOSE)
	{
		if (m_hWnd) PostMessage(m_hWnd, WM_CLOSE, 0, 0);
		return;
	}
	if (btn == BTN_MIN)
	{
		if (m_hWnd) ShowWindow(m_hWnd, SW_MINIMIZE);
		return;
	}

	// Clicking anywhere else on the title bar starts a window drag.
	if (my < TITLE_H)
	{
		RECT wr;
		GetWindowRect(m_hWnd, &wr);
		POINT pt;
		GetCursorPos(&pt);
		m_dragOffset.x = pt.x - wr.left;
		m_dragOffset.y = pt.y - wr.top;
		m_dragging = true;
		SetCapture(m_hWnd);
		return;
	}

	// In the play area: a click restarts after game over, otherwise steers.
	if (m_gameOver)
	{
		ResetGame();
		if (m_hWnd) InvalidateRect(m_hWnd, NULL, FALSE);
		return;
	}
	OnMouseMove(mx, my);
}

void SnakeWindow::OnLButtonUp(int /*mx*/, int /*my*/)
{
	if (m_dragging)
	{
		m_dragging = false;
		ReleaseCapture();
	}
}

void SnakeWindow::OnTick()
{
	if (m_hWnd)
		InvalidateRect(m_hWnd, NULL, FALSE);

	if (m_gameOver || m_paused)
		return;

	// Commit the buffered direction for this step.
	m_dir = m_pendingDir;

	Cell head = m_snake[0];
	switch (m_dir)
	{
	case DIR_UP:    head.y -= 1; break;
	case DIR_DOWN:  head.y += 1; break;
	case DIR_LEFT:  head.x -= 1; break;
	case DIR_RIGHT: head.x += 1; break;
	}

	// Wall collision.
	if (head.x < 0 || head.x >= GRID_W || head.y < 0 || head.y >= GRID_H)
	{
		m_gameOver = true;
		return;
	}

	// Self collision (skip the tail cell, which will move away — unless we grow).
	const bool willGrow = (head.x == m_food.x && head.y == m_food.y);
	const int checkTo = willGrow ? m_length : m_length - 1;
	for (int i = 0; i < checkTo; ++i)
	{
		if (m_snake[i].x == head.x && m_snake[i].y == head.y)
		{
			m_gameOver = true;
			return;
		}
	}

	// Advance the body: shift everyone one step back.
	if (willGrow)
	{
		if (m_length < GRID_W * GRID_H)
			++m_length;
	}
	for (int i = m_length - 1; i > 0; --i)
		m_snake[i] = m_snake[i - 1];
	m_snake[0] = head;

	if (willGrow)
	{
		m_score += 10;
		if (m_score > m_best) m_best = m_score;
		SpawnFood();
	}
}

// ---------------------------------------------------------------------------
// Game logic helpers
// ---------------------------------------------------------------------------

unsigned SnakeWindow::NextRandom()
{
	// xorshift32 — cheap, deterministic, no CRT dependency.
	unsigned x = m_rngState;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	m_rngState = x ? x : 0x1234567u;
	return m_rngState;
}

void SnakeWindow::ResetGame()
{
	m_length = 4;
	const int startX = GRID_W / 2;
	const int startY = GRID_H / 2;
	for (int i = 0; i < m_length; ++i)
	{
		m_snake[i].x = startX - i;
		m_snake[i].y = startY;
	}
	m_dir = DIR_RIGHT;
	m_pendingDir = DIR_RIGHT;
	m_score = 0;
	m_gameOver = false;
	m_paused = false;
	SpawnFood();
}

void SnakeWindow::SetDirection(Dir d)
{
	if (m_gameOver)
		return;

	// Reject a 180° reversal relative to the direction we are actually moving.
	const bool opposite =
		(d == DIR_UP    && m_dir == DIR_DOWN)  ||
		(d == DIR_DOWN  && m_dir == DIR_UP)    ||
		(d == DIR_LEFT  && m_dir == DIR_RIGHT) ||
		(d == DIR_RIGHT && m_dir == DIR_LEFT);
	if (opposite)
		return;

	m_pendingDir = d;
	m_paused = false; // any steering input resumes play
}

void SnakeWindow::SpawnFood()
{
	// Reject placements on top of the snake; retry a bounded number of times.
	for (int attempt = 0; attempt < 500; ++attempt)
	{
		Cell c;
		c.x = (int)(NextRandom() % GRID_W);
		c.y = (int)(NextRandom() % GRID_H);
		bool onSnake = false;
		for (int i = 0; i < m_length; ++i)
		{
			if (m_snake[i].x == c.x && m_snake[i].y == c.y) { onSnake = true; break; }
		}
		if (!onSnake)
		{
			m_food = c;
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void SnakeWindow::OnPaint(HWND hWnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);

	RECT client;
	GetClientRect(hWnd, &client);

	// Double buffer.
	HDC mem = CreateCompatibleDC(hdc);
	HBITMAP bmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
	HBITMAP old = (HBITMAP)SelectObject(mem, bmp);

	Render(mem, client);

	BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);

	SelectObject(mem, old);
	DeleteObject(bmp);
	DeleteDC(mem);
	EndPaint(hWnd, &ps);
}

void SnakeWindow::Render(HDC hdc, const RECT& client)
{
	// Palette.
	const COLORREF colBg     = RGB(18, 22, 28);
	const COLORREF colHeader = RGB(28, 34, 44);
	const COLORREF colField  = RGB(24, 30, 38);
	const COLORREF colGrid   = RGB(34, 42, 52);
	const COLORREF colSnake  = RGB(80, 220, 130);
	const COLORREF colHead   = RGB(150, 255, 180);
	const COLORREF colFood   = RGB(240, 90, 90);
	const COLORREF colText   = RGB(220, 228, 236);
	const COLORREF colDim    = RGB(140, 150, 162);

	// Background.
	HBRUSH bgBrush = CreateSolidBrush(colBg);
	FillRect(hdc, &client, bgBrush);
	DeleteObject(bgBrush);

	// Custom EuroScope-style title bar (drawn first, occupies the top strip).
	DrawTitleBar(hdc);

	// Score header bar (below the title bar).
	RECT header = { 0, TITLE_H, client.right, TITLE_H + HEADER };
	HBRUSH hdrBrush = CreateSolidBrush(colHeader);
	FillRect(hdc, &header, hdrBrush);
	DeleteObject(hdrBrush);

	SetBkMode(hdc, TRANSPARENT);

	HFONT font = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	HFONT oldFont = (HFONT)SelectObject(hdc, font);

	wchar_t buf[128];
	SetTextColor(hdc, colText);
	wsprintfW(buf, L"Score: %d", m_score);
	RECT trScore = { MARGIN, TITLE_H, client.right / 2, TITLE_H + HEADER };
	DrawTextW(hdc, buf, -1, &trScore, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	SetTextColor(hdc, colDim);
	wsprintfW(buf, L"Best: %d", m_best);
	RECT trBest = { client.right / 2, TITLE_H, client.right - MARGIN, TITLE_H + HEADER };
	DrawTextW(hdc, buf, -1, &trBest, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

	// Play field.
	const int fieldX = MARGIN;
	const int fieldY = TITLE_H + HEADER + MARGIN;
	RECT field = { fieldX, fieldY, fieldX + GRID_W * CELL, fieldY + GRID_H * CELL };
	HBRUSH fieldBrush = CreateSolidBrush(colField);
	FillRect(hdc, &field, fieldBrush);
	DeleteObject(fieldBrush);

	// Grid lines.
	HPEN gridPen = CreatePen(PS_SOLID, 1, colGrid);
	HPEN oldPen = (HPEN)SelectObject(hdc, gridPen);
	for (int gx = 0; gx <= GRID_W; ++gx)
	{
		MoveToEx(hdc, fieldX + gx * CELL, fieldY, NULL);
		LineTo(hdc, fieldX + gx * CELL, fieldY + GRID_H * CELL);
	}
	for (int gy = 0; gy <= GRID_H; ++gy)
	{
		MoveToEx(hdc, fieldX, fieldY + gy * CELL, NULL);
		LineTo(hdc, fieldX + GRID_W * CELL, fieldY + gy * CELL);
	}
	SelectObject(hdc, oldPen);
	DeleteObject(gridPen);

	// Food.
	{
		RECT r = {
			fieldX + m_food.x * CELL + 3,
			fieldY + m_food.y * CELL + 3,
			fieldX + (m_food.x + 1) * CELL - 2,
			fieldY + (m_food.y + 1) * CELL - 2
		};
		HBRUSH b = CreateSolidBrush(colFood);
		HPEN p = CreatePen(PS_SOLID, 1, colFood);
		HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
		HPEN op = (HPEN)SelectObject(hdc, p);
		Ellipse(hdc, r.left, r.top, r.right, r.bottom);
		SelectObject(hdc, ob);
		SelectObject(hdc, op);
		DeleteObject(b);
		DeleteObject(p);
	}

	// Snake.
	for (int i = 0; i < m_length; ++i)
	{
		RECT r = {
			fieldX + m_snake[i].x * CELL + 2,
			fieldY + m_snake[i].y * CELL + 2,
			fieldX + (m_snake[i].x + 1) * CELL - 1,
			fieldY + (m_snake[i].y + 1) * CELL - 1
		};
		HBRUSH b = CreateSolidBrush(i == 0 ? colHead : colSnake);
		FillRect(hdc, &r, b);
		DeleteObject(b);
	}

	// Overlays.
	if (m_paused && !m_gameOver)
	{
		SetTextColor(hdc, colText);
		RECT r = field;
		DrawTextW(hdc, L"PAUSED\n(Space to resume)", -1, &r,
			DT_CENTER | DT_VCENTER | DT_WORDBREAK);
	}
	if (m_gameOver)
	{
		// Dim the field.
		RECT r = field;
		HFONT big = CreateFontW(34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
		HFONT of = (HFONT)SelectObject(hdc, big);
		SetTextColor(hdc, colFood);
		RECT rTitle = field;
		rTitle.bottom = field.top + (field.bottom - field.top) / 2;
		DrawTextW(hdc, L"GAME OVER", -1, &rTitle, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
		SelectObject(hdc, of);
		DeleteObject(big);

		SetTextColor(hdc, colText);
		RECT rHint = field;
		rHint.top = field.top + (field.bottom - field.top) / 2 + 8;
		wsprintfW(buf, L"Score: %d\n(Space / click to restart)", m_score);
		DrawTextW(hdc, buf, -1, &rHint, DT_CENTER | DT_TOP | DT_WORDBREAK);
	}

	SelectObject(hdc, oldFont);
	DeleteObject(font);

	// Thin outer window border (since WS_POPUP has no frame of its own).
	HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(90, 96, 104));
	HPEN oldBorderPen = (HPEN)SelectObject(hdc, borderPen);
	HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
	HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
	Rectangle(hdc, client.left, client.top, client.right, client.bottom);
	SelectObject(hdc, oldBrush);
	SelectObject(hdc, oldBorderPen);
	DeleteObject(borderPen);
}

// ---------------------------------------------------------------------------
// Custom EuroScope-style title bar
// ---------------------------------------------------------------------------

void SnakeWindow::DrawTitleBar(HDC hdc)
{
	// Colours picked to match EuroScope's window chrome: a flat medium-grey
	// caption strip with white text and small square buttons on the right.
	const COLORREF colBar      = RGB(74, 78, 86);
	const COLORREF colBarLight = RGB(96, 100, 110);
	const COLORREF colBarLine  = RGB(40, 43, 49);
	const COLORREF colTitle    = RGB(232, 236, 240);
	const COLORREF colGlyph    = RGB(232, 236, 240);
	const COLORREF colHover    = RGB(96, 100, 110);
	const COLORREF colClose     = RGB(196, 64, 60);

	RECT bar = { 0, 0, CLIENT_W, TITLE_H };

	// Bar background.
	HBRUSH barBrush = CreateSolidBrush(colBar);
	FillRect(hdc, &bar, barBrush);
	DeleteObject(barBrush);

	// Subtle 1px highlight along the top edge, and a divider line at the bottom.
	HPEN topPen = CreatePen(PS_SOLID, 1, colBarLight);
	HPEN oldPen = (HPEN)SelectObject(hdc, topPen);
	MoveToEx(hdc, 0, 0, NULL);           LineTo(hdc, CLIENT_W, 0);
	SelectObject(hdc, oldPen);
	DeleteObject(topPen);

	HPEN linePen = CreatePen(PS_SOLID, 1, colBarLine);
	oldPen = (HPEN)SelectObject(hdc, linePen);
	MoveToEx(hdc, 0, TITLE_H - 1, NULL); LineTo(hdc, CLIENT_W, TITLE_H - 1);
	SelectObject(hdc, oldPen);
	DeleteObject(linePen);

	// Title text.
	SetBkMode(hdc, TRANSPARENT);
	HFONT titleFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
	SetTextColor(hdc, colTitle);
	RECT trTitle = { 8, 0, CLIENT_W - 2 * BTN_W - 4, TITLE_H };
	DrawTextW(hdc, L"EuroScope Snake", -1, &trTitle,
		DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	SelectObject(hdc, oldFont);
	DeleteObject(titleFont);

	// Button backgrounds on hover.
	RECT rMin   = { CLIENT_W - 2 * BTN_W, 0, CLIENT_W - BTN_W, TITLE_H - 1 };
	RECT rClose = { CLIENT_W - BTN_W,     0, CLIENT_W,         TITLE_H - 1 };

	if (m_hoverBtn == BTN_MIN)
	{
		HBRUSH b = CreateSolidBrush(colHover);
		FillRect(hdc, &rMin, b);
		DeleteObject(b);
	}
	if (m_hoverBtn == BTN_CLOSE)
	{
		HBRUSH b = CreateSolidBrush(colClose);
		FillRect(hdc, &rClose, b);
		DeleteObject(b);
	}

	// Glyphs.
	HPEN glyphPen = CreatePen(PS_SOLID, 1, colGlyph);
	oldPen = (HPEN)SelectObject(hdc, glyphPen);

	// Minimize: a short horizontal bar near the vertical centre.
	{
		const int cx = (rMin.left + rMin.right) / 2;
		const int cy = TITLE_H / 2 + 3;
		MoveToEx(hdc, cx - 5, cy, NULL);
		LineTo(hdc, cx + 5, cy);
	}
	// Close: an X.
	{
		const int cx = (rClose.left + rClose.right) / 2;
		const int cy = TITLE_H / 2;
		MoveToEx(hdc, cx - 4, cy - 4, NULL); LineTo(hdc, cx + 5, cy + 5);
		MoveToEx(hdc, cx + 4, cy - 4, NULL); LineTo(hdc, cx - 5, cy + 5);
	}

	SelectObject(hdc, oldPen);
	DeleteObject(glyphPen);
}
