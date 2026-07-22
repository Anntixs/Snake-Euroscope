#pragma once

//
// SnakeWindow.h
//
// A self-contained Win32 window that runs the Snake game.  The window lives on
// its own thread with its own message loop, so it is completely independent of
// EuroScope's UI thread.  The plugin only talks to it through Open()/Close().
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

class SnakeWindow
{
public:
	SnakeWindow();
	~SnakeWindow();

	// Opens the game window (spawns the window thread).  A no-op if already open.
	void Open();

	// Requests the window to close.  A no-op if already closed.
	void Close();

	// Toggles between open and closed.
	void Toggle();

	// True while the window thread is alive.
	bool IsOpen() const;

private:
	// ---- grid / geometry --------------------------------------------------
	static const int GRID_W = 24;      // cells across
	static const int GRID_H = 24;      // cells down
	static const int CELL   = 22;      // pixel size of one cell
	static const int HEADER = 40;      // pixels reserved for the score bar
	static const int MARGIN = 12;      // border around the play field

	enum Dir { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

	struct Cell { int x; int y; };

	// ---- window plumbing --------------------------------------------------
	static DWORD WINAPI ThreadProc(LPVOID param);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void RunMessageLoop();
	void OnCreate(HWND hWnd);
	void OnDestroy();
	void OnPaint(HWND hWnd);
	void OnKeyDown(WPARAM key);
	void OnMouseMove(int mx, int my);
	void OnLButtonDown(int mx, int my);
	void OnTick();

	// ---- game logic -------------------------------------------------------
	void ResetGame();
	void SetDirection(Dir d);
	void SpawnFood();
	void Render(HDC hdc, const RECT& client);

	// ---- state ------------------------------------------------------------
	HANDLE m_thread;
	HWND   m_hWnd;
	volatile bool m_open;

	Cell   m_snake[GRID_W * GRID_H];
	int    m_length;
	Dir    m_dir;
	Dir    m_pendingDir;     // buffered so a fast double-turn can't reverse us
	Cell   m_food;
	int    m_score;
	int    m_best;
	bool   m_gameOver;
	bool   m_paused;
	UINT_PTR m_timer;
	unsigned m_rngState;

	unsigned NextRandom();
};
