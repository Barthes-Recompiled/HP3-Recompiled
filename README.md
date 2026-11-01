# Harry Potter and the Prisoner of Azkaban — Xbox to PC port (beta)

A native Windows port of the original Xbox game *Harry Potter and the Prisoner of Azkaban* (2004), in beta, made
by **static recompilation**: the game's x86 code is translated to C and compiled for Windows, with the Xbox kernel,
Direct3D 8, DirectSound and input replaced by native code. No emulator. Part of
[Barthes Recompiled](https://github.com/Barthes-Recompiled), beside
[Philosopher's Stone](https://github.com/Barthes-Recompiled/HP1-Recompiled) and
[Chamber of Secrets](https://github.com/Barthes-Recompiled/HP2-Recompiled).

**Current release: 0.5.2 Beta**, for the USA and PAL discs. New Game plays through the Hogwarts Express into the
first level at 60 fps in true 16:9, with saving and loading; later levels haven't been checked yet (see
[the beta notes](#beta-what-works-and-what-doesnt-yet)). It stands entirely on
[MiddleDucky's work](#thank-you-middleducky).

**This repository contains no game content.** To play or build it you need the files from your own copy of the
Xbox disc. The build regenerates the recompiled code from your `default.xbe`.

## Quick start

1. Download **`BarthesLauncher-windows-<version>.zip`** from the
   [Barthes Launcher releases](https://github.com/Barthes-Recompiled/Barthes-Launcher/releases/latest) and unzip it
   somewhere of its own, for example `C:\Games` (it makes a `BarthesLauncher` folder).
2. Start `barthes_launcher.exe`, pick **Prisoner of Azkaban** on the shelf and press **Install**.
3. Press **Add disc image or folder...** and choose your `.iso` / `.xiso`, or the `default.xbe` of a disc you
   already extracted. The launcher checks that it is this game's disc, unpacks it and installs the build for your
   disc (PAL or USA).
4. Press **Play**.

The launcher keeps the port up to date from this repository's releases and never touches your disc files, saves or
settings when it updates.

## Setting up by hand

1. Download the [latest release](https://github.com/Barthes-Recompiled/HP3-Recompiled/releases/latest) for your
   disc: **USA** for the North American disc, **PAL** for the European one. Unzip it
   into a folder of its own, for example `C:\Games\HP3`.
2. Make a folder named `game` beside `hppoa.exe` and put your disc's files in it, so that `game\default.xbe` exists.
   A disc image has to be extracted first (the Barthes Launcher does this for you; [extract-xiso](https://github.com/XboxDev/extract-xiso)
   works too).
3. Start `hppoa.exe`.

```
C:\Games\HP3\
  hppoa.exe
  README.txt, CHANGES.txt, licenses\
  shadercache\         shaders (shipped ones, plus those built on your PC)
  game\default.xbe     ... and the rest of your disc
  hppoa.toml           your settings (made on the first start)
  saves\               your saved games, and the game's Xbox disk images in saves\xbox
```

To update by hand, unzip the new release over the old one; `hppoa.toml`, `saves\` and `shadercache\` stay as they
are. The whole folder is portable: move it or back it up as it is.

**Requirements:** Windows 10 or 11 (64-bit), a DirectX 12 graphics card (GeForce GTX 600, Radeon HD 7000, Intel HD
500 or newer) with a current driver, 4 GB RAM, about 3.1 GB of disk. Keyboard and mouse, or an Xbox 360 / One /
Series controller (other pads through Steam Input or DS4Windows).

## Which disc

Both editions work: the North American (NTSC-U) disc and the European (PAL) disc, which boots to its language
screen. A build is made for one disc, so each release has a `USA` and a `PAL` download; the Barthes Launcher reads
your disc's `default.xbe` and installs the right one.

## Settings

- **F10** anywhere: every option, over the game, with the same tabs and rows as the sibling ports (Graphics, Audio,
  Controls, Gameplay, Accessibility, Debug).
- **PC Settings** while the game is paused: press **Back** (View/Select on a modern pad) for the display, frame rate,
  anti-aliasing, controller and keyboard settings, drawn in the game's own menu area. Up/down picks a row, left/right
  changes it, B or Back closes.
- **Alt+Enter** switches windowed / borderless; **Alt+F4** quits.

Both save to `hppoa.toml`. Highlights: 60 fps by default with the game running at the right speed (30 fps and higher
caps too); resolutions up to 4K with MSAA, SMAA and anisotropic filtering; true 16:9 with menus kept at 4:3 and the
original cutscene letterbox; windowed, borderless or fullscreen; keyboard and mouse with rebinding; controller
remapping, deadzones and vibration; colour-blind correction filters. Every option and the default keys are listed in
[port/README.txt](port/README.txt), which also ships in the release.

## Beta: what works and what doesn't yet

- **Works:** the title screen and attract loop, New Game through the Hogwarts Express into the first level, saving
  and loading, sound, 60 fps, 16:9, both discs.
- **Not checked yet:** levels after the first. Reports from further in are very welcome.
- **Not there yet:** Quit To Desktop (use Alt+F4), a Settings page on the title menu (use F10 there), cutscene
  fast-forward and the start-up logo skip (shown greyed out). Button prompts are always Xbox's.

## Archipelago

An [Archipelago](https://archipelago.gg) multiworld randomizer for Prisoner of Azkaban is being built and is coming
soon. When it lands, it will be set up the same way as Philosopher's Stone's: on the game's Archipelago page in the
Barthes Launcher.

## Troubleshooting

| Problem | What to do |
|---|---|
| "Failed to load default.xbe" | The disc files are missing: check that `game\default.xbe` exists beside `hppoa.exe`. |
| The game says it is the wrong disc | Download the other build (`PAL` or `USA`), or let the Barthes Launcher pick it. |
| The game won't start or closes at once | Update your graphics driver (DirectX 12 is required). Every start writes `hppoa_run.log` beside `hppoa.exe`; send it with your report. For more detail turn on F10 > Debug > *Write debug logs* and start again. |
| A short pause the first time you enter an area | Shaders are being built for your card, once per area; later starts prepare them ahead in the menus. |
| Keyboard doesn't move Harry while a pad is plugged in | Turn off F10 > Controls > *Use controller*. |

Problems and questions: [open an issue](https://github.com/Barthes-Recompiled/HP3-Recompiled/issues). What changed
in each release is in [CHANGES.md](CHANGES.md).

## Building

See [port/DEVELOPING.md](port/DEVELOPING.md): MSYS2 (UCRT64) setup, regenerating the code from the XBE, building,
debugging tools and how the port works. The design and plans are in [docs/superpowers/](docs/superpowers/).

## Thank you, MiddleDucky

This port exists only because of **[MiddleDucky](https://github.com/MiddleDucky)**, who made the Philosopher's Stone and Chamber of Secrets Xbox ports this one is built from
([HP-PS-Xbox-Recomp](https://github.com/MiddleDucky/HP-PS-Xbox-Recomp),
[HP-COS-Xbox-Recomp](https://github.com/MiddleDucky/HP-COS-Xbox-Recomp)): the port layer, renderer adaptation, audio,
input, launcher, trainer, region retargeting and much of the toolkit work. MiddleDucky has also started a Prisoner of
Azkaban port of their own ([HP-POA-Xbox-Recomp](https://github.com/MiddleDucky/HP-POA-Xbox-Recomp)), and two fixes in
this toolkit come from their work on PoA: the factory AV region answer (PoA times its frames by the video standard)
and NV2A-style line drawing (PoA's shadow fade mask relies on it). Thank you, MiddleDucky, wholeheartedly: none of
this exists without you. If you enjoy this port, go and star MiddleDucky's repositories.

## Credits and licences

The port's own code is MIT licensed, © 2026 MiddleDucky and Barthes Recompiled contributors ([LICENSE](LICENSE)).
The parts below keep their own licences and copyright.

* **xboxrecomp** — the static recompilation toolkit this port is built on (runtime, kernel layer, pipeline),
  MIT, by sp00nz. Its own readme: [README.xboxrecomp.md](README.xboxrecomp.md); licence [LICENSES/xboxrecomp-MIT.txt](LICENSES/xboxrecomp-MIT.txt);
  third-party parts (including LGPL code from xemu) in [NOTICE](NOTICE) and [LICENSES/](LICENSES/).
* **SCDA-XBERecomp** by TGP482 — the D3D8-to-Direct3D 12 renderer the port's graphics are based on, MIT
  ([port/src/scda/LICENSE.SCDA](port/src/scda/LICENSE.SCDA)).
* **Dear ImGui** (MIT, port/third_party/imgui) and **SMAA** (MIT, Jorge Jimenez et al.).

### Thanks

These projects were references for hardware behaviour, XDK details or fixes
(no code taken unless listed above):

* [xemu](https://github.com/xemu-project/xemu) — Xbox hardware behaviour; the APU code listed in NOTICE
* [XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase) — XDK function addresses
* [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded) and [xboxdevwiki](https://xboxdevwiki.net) — kernel and hardware reference
* [Nightfire Windows Port](https://github.com/limeylicious/Nightfire-Windows-Port) — the same lifter bugs, found first
* [DefJamFFNY-recomp](https://github.com/vyanhursky/DefJamFFNY-recomp) — single-core guest thread pinning
* [Unicorn Engine](https://github.com/unicorn-engine/unicorn) — the debug-only difftest build

*Harry Potter* and all related names and artwork are trademarks of and © Warner Bros. Entertainment Inc. The game
is © Electronic Arts. This is an unofficial fan project, not affiliated with or endorsed by Warner Bros.,
Electronic Arts or Microsoft.

This project has been aided by Claude Code.
