#pragma once

//
// SnakePlugin.h
//
// The EuroScope plugin entry point.  It registers a text command (".snake")
// that opens, closes or toggles the standalone Snake game window.
//

// The EuroScope SDK header uses Win32 types (POINT, RECT, COLORREF, NULL) but
// does not include <windows.h> itself, so we must pull it in first.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "EuroScopePlugIn.h"
#include "SnakeWindow.h"

class SnakePlugin : public EuroScopePlugIn::CPlugIn
{
public:
	SnakePlugin();
	virtual ~SnakePlugin();

	// Handles ".snake" dot-commands typed into the EuroScope command line.
	virtual bool OnCompileCommand(const char* sCommandLine);

private:
	SnakeWindow m_window;
	bool        m_greeted;   // whether the one-time welcome message was shown
};
