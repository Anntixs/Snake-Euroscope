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
{
	m_food.x = m_food.y = 0;
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

	const int clientW = MARGIN * 2 + GRID_W * CELL;
	const int clientH = HEADER + MARGIN * 2 + GRID_H * CELL;

	RECT rc = { 0, 0, clientW, clientH };
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	AdjustWindowRect(&rc, style, FALSE);

	HWND hWnd = CreateWindowExW(
		0, kClassName, kWindowTitle, style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
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

void SnakeWindow::OnMouseMove(int mx, int my)
{
	// Steer the snake toward the mouse cursor: pick the dominant axis of the
	// vector from the head to the pointer.  This gives smooth mouse control
	// without needing clicks.
	if (m_gameOver || m_paused || m_length == 0)
		return;

	const int headPxX = MARGIN + m_snake[0].x * CELL + CELL / 2;
	const int headPxY = HEADER + MARGIN + m_snake[0].y * CELL + CELL / 2;

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
	// A click restarts after game over, or (during play) also steers.
	if (m_gameOver)
	{
		ResetGame();
		if (m_hWnd) InvalidateRect(m_hWnd, NULL, FALSE);
		return;
	}
	OnMouseMove(mx, my);
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

	// Header bar.
	RECT header = { 0, 0, client.right, HEADER };
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
	RECT trScore = { MARGIN, 0, client.right / 2, HEADER };
	DrawTextW(hdc, buf, -1, &trScore, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	SetTextColor(hdc, colDim);
	wsprintfW(buf, L"Best: %d", m_best);
	RECT trBest = { client.right / 2, 0, client.right - MARGIN, HEADER };
	DrawTextW(hdc, buf, -1, &trBest, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

	// Play field.
	const int fieldX = MARGIN;
	const int fieldY = HEADER + MARGIN;
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
}
