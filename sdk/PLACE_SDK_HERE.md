# Put the EuroScope SDK here

This folder is where the build looks for the EuroScope plugin SDK by default
(`-DEUROSCOPE_SDK` defaults to `./sdk`).

Copy these two files from your EuroScope installation into this folder:

* `EuroScopePlugIn.h`
* `EuroScopePlugIn.lib`

They ship with EuroScope (the SDK Gergely releases alongside each EuroScope
version). They are **not** redistributed in this repository — see the root
`README.md` for details.

Once both files are present here you can build with:

```powershell
cmake -A Win32 -B build
cmake --build build --config Release
```
