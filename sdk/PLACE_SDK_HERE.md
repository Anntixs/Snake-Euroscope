# EuroScope SDK

This folder holds the EuroScope plugin SDK that the build links against
(`-DEUROSCOPE_SDK` defaults to `./sdk`):

* `EuroScopePlugIn.h`   — the SDK header
* `EuroScopePlugInDll.lib` — the import library (x86)

These files are committed so that GitHub Actions (and a fresh clone) can build
the plugin without any extra setup. They ship with EuroScope itself; if you ever
need to update them, copy the newer versions from your EuroScope installation
over the ones here.
