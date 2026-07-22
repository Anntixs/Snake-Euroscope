#pragma once

//
// SnakePlugin.h
//
// The EuroScope plugin entry point.  It registers a text command (".snake")
// that opens, closes or toggles the standalone Snake game window.
//

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
};
