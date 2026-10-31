HARRY POTTER AND THE PRISONER OF AZKABAN - PC PORT (BETA)
=========================================================

A native Windows port of the original Xbox game, made by static recompilation.
It contains no game data: you need the files from your own copy of the Xbox disc.

BETA (0.5.0). Tested from New Game through the Hogwarts Express and the
first level, with saving and loading, at 30 and 60 fps. Later levels have not
been checked yet. Works with the North American (NTSC-U) disc only.

Not in this beta yet: settings pages inside the game's menus, "Quit To
Desktop" in the menus (use Alt+F4), and cutscene fast-forward (its option is
there but greyed out, as is the start-up logo skip).


MINIMUM SYSTEM REQUIREMENTS
---------------------------

  OS:       Windows 10 or 11, 64-bit
  CPU:      4 threads with SSE4.1 (Intel Core 2nd generation / AMD FX or newer)
  Memory:   4 GB RAM
  Graphics: DirectX 12 card (feature level 11_0) with a current driver:
            NVIDIA GeForce GTX 600 series, AMD Radeon HD 7000 series,
            Intel HD Graphics 500 series or newer
  Storage:  about 3.1 GB (3 GB game files + the port)
  Input:    keyboard and mouse, or an Xbox controller (360, One or Series)

Windows "N" editions also need Microsoft's Media Feature Pack.


INSTALLING
----------

1. Put the port's files in a folder of your choice:
       hppoa_launcher.exe
       hppoa.exe
       README.txt
       licenses\
       shadercache\base_shaders.bin
2. Copy the game files from your disc or the .xiso file into a folder named "game" in the same folder with the executables.

Updating from an earlier version: replace the files above and keep your
hppoa.toml, saves\ and shadercache\ (your settings, saved games and the
shaders already built on your PC stay as they are).

The port creates these itself on the first run:
    hppoa.toml      your settings (the launcher and F10 change them)
    saves\         your saved games
    shadercache\   shaders built on your PC (the first visit to an area
                   prepares them; later visits load at once)


PLAYING
-------

Start the game with hppoa_launcher.exe. Opening hppoa.exe directly only offers
to open the launcher.

The launcher sets the aspect ratio (4:3 or 16:9), display, graphics and
controls, then starts the game with Play. If you turn its page off, the
launcher starts the game straight away; hold Shift while opening it to see
the page again.

Only one copy of the game runs at a time: starting it again brings the
running game to the front.

Quitting: Alt+F4 (a "Quit To Desktop" menu entry comes in a later version).

Keys that always work
  F10          Options (see below)
  Alt+Enter    switch between windowed and borderless
  Alt+F4       quit the game

Controller
  An Xbox 360 / One / Series controller plays like the original Xbox pad.
  The original pad's White and Black buttons are LB and RB.
  To play with keyboard and mouse while a controller is connected, turn
  off "Use controller" (launcher or F10 > Controls).

Keyboard and mouse (defaults; change them under F10 > Controls)
  Move                  W A S D
  Camera                mouse, or the arrow keys
  A                     Space
  B                     E
  X                     left mouse button
  Y                     right mouse button
  Black / White         R / Q
  Left / right trigger  Shift / Ctrl
  D-pad                 1 2 3 4 (up, down, left, right)
  Start / Back          Escape / Tab
  Stick clicks          C (left), middle mouse button (right)
  Sneak (toggle)        Caps Lock: Harry keeps his sneaking pace until you
                        press it again ("Sneaking" shows in the corner)
  Pace of the keys      mouse wheel: down Sneak < Walk < Run up (Run is the
                        default; also F10 > Controls > Movement keys pace)

Sneaking: on a controller, push the left stick gently. Keys always move at
full speed, so use the sneak toggle; a controller button can toggle it too
(F10 > Controls > Sneak toggle button).


OPTIONS (F10)
-------------

F10 opens the options over the game; most changes apply at once and are saved
to hppoa.toml. "Defaults (this page)" resets the page you are on (Graphics,
Controls, ...) and leaves the others; the aspect ratio stays. "Show the
launcher at startup" turns the launcher's page on or off. Click the arrows
or use left/right on a row to change its value. Descriptions wrap and the
rows scroll above the footer, including in a 640x480 window.

Graphics
  Display
    Aspect ratio         4:3 (black bars at the sides), 16:9 (the game's
                         camera widened; menus and movies keep 4:3) or
                         Stretch. Applies at the next start.
    Display mode         Windowed, Borderless (covers the monitor) or
                         Fullscreen (switches the monitor's resolution)
    Resolution           the window's size, or the fullscreen mode; Desktop
                         uses the monitor's own
    Vsync                waits for the monitor's refresh (no tearing)
    Frame rate           60 fps (default), the game's rate (30 fps, as on the Xbox), 30, 120-240
                         or Unlocked; the game keeps its speed at any rate
  Resolution
    Internal resolution  the lines the 3D is drawn at: 480 is the Xbox's own;
                         720 to 2160 sharpen it; "Follow window" matches the
                         window
    Render scale         with "Follow window": the render size as a share of
                         the window's
    Max render height    with "Follow window": the most lines it follows the
                         window to (1080 by default, so a 4K screen keeps
                         60 fps on modest cards)
  Anti-aliasing and filtering
    MSAA                 smooths polygon edges: off, 2x, 4x, 8x
    SMAA                 smooths edges over the finished picture; combines
                         with MSAA
    Anisotropic filtering  sharper textures at an angle: the game's own, 2x,
                         4x, 8x or 16x
  Picture
    Brightness, Contrast 50 leaves the picture as the game draws it
    FPS counter          shows the frame rate

Audio
  Speakers               Stereo, or 5.1 surround: the console's own speaker
                         mix, each channel to its own speaker. Needs Windows'
                         speaker setup set to 5.1 (or a 5.1 HDMI receiver).
                         Applies at the next start.

Controls
  Controller
    Use controller       off: the game ignores the controller (and it does
                         not vibrate); keyboard and mouse still play
    Controller in background  on: the controller still plays while another
                         window has the focus; off: it is ignored
    Left / right stick deadzone        how far a stick moves before it counts
    Left stick sensitivity / Camera speed  the response past the deadzone
    Invert camera X / Y
    Sneak toggle button  a controller button that toggles sneaking (it then
                         does nothing else), or None
    Vibration, Vibration strength
    Button remapping     which pad button each game button reads;
                         "Default buttons" restores them
  Keyboard and mouse
    Keyboard and mouse   plays as controller 1 alongside a controller
    Mouse sensitivity, Invert mouse Y
    Movement keys pace   the pace W A S D move Harry at: Sneak, Walk or Run;
                         the mouse wheel steps it in game
    Key bindings         click a binding, then press a key or mouse button
                         (Escape clears it); "Default keys" restores them

Gameplay
  Cutscene fast-forward, Skip start-up logos
                         greyed out: "Not working in this game yet"
  Cutscene bars (16:9)    keeps the original bars over widescreen cutscenes

Accessibility
  Colour blindness
    Correction filter    Off, Protanopia (red-weak), Deuteranopia
                         (green-weak) or Tritanopia (blue-weak): shifts the
                         colours you confuse toward ones you can tell apart
    Strength

Debug
  Logs
    Write debug logs     writes hppoa_run.log and xbox_kernel.log beside the
                         game from the next start. Off by default; turn it on
                         to report a problem, and send both files with your
                         report.
  Developer menu
    Allow the debug menu  the developers' chapter and room select


TROUBLESHOOTING
---------------

"Failed to load default.xbe"
    The game files are missing: check that game\default.xbe exists beside
    hppoa.exe.

The game will not start / closes at once
    Update your graphics driver (DirectX 12 is required). Then turn on
    F10 > Debug > Write debug logs (or set debug_logs = true in hppoa.toml),
    start again and keep hppoa_run.log.

A short pause the first time you enter an area
    Shaders are being prepared for your graphics card; the loading picture
    stays up meanwhile. It happens once per area. From the next start the
    port prepares everything it has seen ahead, while you are in the menus
    ("Preparing shaders" shows in the corner), so those areas load at once.


LICENCES AND SOURCE CODE
------------------------

The port's own code is under the MIT licence. It also contains code from the
xemu project (the Xbox audio processor emulation), which stays under the GNU
Lesser General Public License, version 2.1 or later. The licence texts and the
list of third-party code are in the licenses folder beside this file.

The complete source code, including that code and any changes made to it, is
available at:
    https://github.com/MiddleDucky/HP-PS-Xbox-Recomp

The Harry Potter game itself is not included and is not covered by these
licences.
