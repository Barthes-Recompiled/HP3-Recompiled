# Settings UI layout check

Run `bash tests/settings_ui/run.sh` from MSYS2 UCRT64. This compiles a headless
ImGui harness; it does not launch the game or create a native window.

Checks all six tabs in F10 and the startup panel at 640x480, 1024x768, 1280x720,
1920x1080 and 3840x2160, using both aspect settings, borderless/windowed,
controller/background input on/off, expanded binding trees and key capture.
Assertions check panel and footer bounds, horizontal overflow, label/value
separation and value widths. Game rendering and actual input capture still
need an in-game screenshot/play check.
