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

## Building

EuroScope is a **32-bit** application, so the plugin **must** be compiled for
**x86 / Win32**. You need the EuroScope plugin SDK, which ships with EuroScope.

### 1. Get the SDK

The SDK is just two files:

* `EuroScopePlugIn.h`
* `EuroScopePlugIn.lib`

They come with your EuroScope installation (look in the EuroScope program
folder / the `EuroScopeSDK` that Gergely publishes with each release). Copy both
files into a folder — e.g. `sdk/` next to this README.

### 2. Build with CMake (Visual Studio toolchain)

```powershell
cmake -A Win32 -DEUROSCOPE_SDK="C:/path/to/EuroScopeSDK" -B build
cmake --build build --config Release
```

This produces `build/Release/EuroScopeSnake.dll`.

> If you place the SDK in `./sdk`, you can omit `-DEUROSCOPE_SDK`.

### 3. Install into EuroScope

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
