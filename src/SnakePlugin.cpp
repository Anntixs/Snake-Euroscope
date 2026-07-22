//
// SnakePlugin.cpp
//
// EuroScope plugin glue.  Exposes the required EuroScopePlugInInit /
// EuroScopePlugInExit exports and wires the ".snake" command to the game
// window.
//

#include "SnakePlugin.h"
#include <string>
#include <cstring>
#include <cctype>

// ---- plugin metadata (shown in EuroScope's plugin list) -------------------
static const char* PLUGIN_NAME    = "Snake Game";
static const char* PLUGIN_VERSION = "1.0.0";
static const char* PLUGIN_AUTHOR  = "slaywick";
static const char* PLUGIN_LICENSE = "MIT";

// Single global instance, handed to EuroScope on init.
static SnakePlugin* g_pPlugin = NULL;

// Case-insensitive helper for trimming/comparing the command line.
static std::string ToLowerTrim(const char* s)
{
	std::string out;
	if (!s) return out;
	// left trim
	while (*s && isspace((unsigned char)*s)) ++s;
	out = s;
	// right trim
	while (!out.empty() && isspace((unsigned char)out.back()))
		out.pop_back();
	for (size_t i = 0; i < out.size(); ++i)
		out[i] = (char)tolower((unsigned char)out[i]);
	return out;
}

SnakePlugin::SnakePlugin()
	: CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
	          PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR, PLUGIN_LICENSE)
	, m_greeted(false)
{
	// IMPORTANT: do NOT call any EuroScope API (DisplayUserMessage, etc.) here.
	// During the CPlugIn constructor EuroScope has not finished registering the
	// plugin instance yet, so calling back into it crashes EuroScope on load.
	// The welcome message is deferred to the first OnCompileCommand instead.
}

SnakePlugin::~SnakePlugin()
{
	m_window.Close();
}

bool SnakePlugin::OnCompileCommand(const char* sCommandLine)
{
	const std::string cmd = ToLowerTrim(sCommandLine);

	// We only react to commands that start with ".snake".
	if (cmd.compare(0, 6, ".snake") != 0)
		return false;

	// Show a one-time welcome (safe here — the plugin is fully registered now).
	if (!m_greeted)
	{
		m_greeted = true;
		DisplayUserMessage("Snake", "Snake",
			"Type .snake to open the game, .snake close to hide it.",
			true, true, false, false, false);
	}

	// Everything after ".snake" is the (optional) argument.
	std::string arg = cmd.substr(6);
	// trim leading spaces of the argument
	size_t p = arg.find_first_not_of(' ');
	arg = (p == std::string::npos) ? std::string() : arg.substr(p);

	if (arg.empty() || arg == "toggle")
	{
		m_window.Toggle();
	}
	else if (arg == "open" || arg == "start" || arg == "show")
	{
		m_window.Open();
	}
	else if (arg == "close" || arg == "stop" || arg == "hide" || arg == "quit")
	{
		m_window.Close();
	}
	else
	{
		DisplayUserMessage("Snake", "Snake",
			"Usage: .snake [open|close|toggle]", true, true, false, false, false);
		return true;
	}

	return true; // command consumed
}

// ---------------------------------------------------------------------------
// EuroScope required exports
// ---------------------------------------------------------------------------

void __declspec(dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
	g_pPlugin = new SnakePlugin();
	*ppPlugInInstance = g_pPlugin;
}

void __declspec(dllexport) EuroScopePlugInExit(void)
{
	if (g_pPlugin)
	{
		delete g_pPlugin;
		g_pPlugin = NULL;
	}
}
