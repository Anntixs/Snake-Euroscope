# EuroScope Snake 🐍

A classic Snake game as a **EuroScope plugin**. It opens in its own separate
window that you show or hide with a command, and you steer with the **arrow
keys** (or WASD) **or the mouse**.

![type](https://img.shields.io/badge/type-EuroScope%20plugin-blue)
![arch](https://img.shields.io/badge/arch-x86%20(Win32)-orange)

## What it does

* Adds the command **`.snake`** to EuroScope's command line.
* The game runs in a **standalone Win32 window** on its own thread, so it never
  blocks or interferes with EuroScope's radar display.
* Open / close the window at any time with a command.

## Commands

Type these into the EuroScope command line (bottom of the screen):

| Command          | Action                                  |
| ---------------- | --------------------------------------- |
| `.snake`         | Toggle the window open/closed           |
| `.snake open`    | Open the game window                     |
| `.snake close`   | Close the game window                    |
| `.snake toggle`  | Same as `.snake`                         |

## Controls

| Input                       | Action                          |
| --------------------------- | ------------------------------- |
| **Arrow keys** / **WASD**   | Steer the snake                 |
| **Mouse move / click**      | Steer toward the cursor         |
| **Space**                   | Pause / resume (restart if over)|
| **R**                       | Restart                         |
| **Esc**                     | Close the window                |
| **Click** (after game over) | Restart                         |

Eat the red food to grow and score. Hitting a wall or yourself ends the game.

## Download a prebuilt DLL (easiest)

Every push is built on GitHub Actions. To grab the compiled plugin:

1. Open the **Actions** tab of this repository.
2. Click the most recent **"Build EuroScope Snake plugin"** run.
3. Download the **`EuroScopeSnake-dll`** artifact — it contains
   `EuroScopeSnake.dll`.

Then jump to [Install into EuroScope](#install-into-euroscope).

## Building locally

EuroScope is a **32-bit** application, so the plugin **must** be compiled for
**x86 / Win32**. The EuroScope SDK (`EuroScopePlugIn.h` and
`EuroScopePlugInDll.lib`) is already committed under `sdk/`, so no extra setup
is needed:

```powershell
cmake -A Win32 -B build
cmake --build build --config Release
```

This produces `build/Release/EuroScopeSnake.dll`.

> To build against a different SDK location, pass
> `-DEUROSCOPE_SDK="C:/path/to/EuroScopeSDK"`.

## Install into EuroScope

1. Open EuroScope.
2. Go to **OTHER SET → Plug-ins…**.
3. Click **Load** and select `EuroScopeSnake.dll`.
4. Close the dialog. The plugin prints a "Loaded" message in the chat.
5. Type `.snake` to play.

## Notes

* The DLL and EuroScope must have the **same bitness** (x86). A 64-bit build
  will fail to load.
* The window is fully independent — you can keep it open while working traffic,
  or `.snake close` it instantly.

## License

MIT — see [LICENSE](LICENSE).
