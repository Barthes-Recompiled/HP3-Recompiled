# Harry Potter and the Prisoner of Azkaban — Xbox to PC port (beta)

A native Windows port of the original Xbox game *Harry Potter and the Prisoner of Azkaban* (2004), in beta, made
by **static recompilation**: the game's x86 code is translated to C and compiled for Windows, with the Xbox kernel,
Direct3D 8, DirectSound and input replaced by native code. No emulator. It is the third of the barthes_recompiled
Harry Potter ports, beside Philosopher's Stone (hp1_recomp) and Chamber of Secrets (hp2_recomp).

**State: 0.5.0 Beta.** New Game plays through the Hogwarts Express into the first level at 60 fps in true 16:9,
with saving and loading; later levels haven't been checked yet. What works and the known issues are in
[CHANGES.md](CHANGES.md). Still to come: settings pages inside the game's menus, Quit To Desktop, cutscene
fast-forward, the European disc, and an Archipelago randomizer (coming soon).

## Playing

The easiest way is the **[Barthes Launcher](https://github.com/Barthes-Recompiled/Barthes-Launcher/releases/latest)**,
one launcher for all three Barthes Recompiled Harry Potter ports: it installs the port, unpacks your disc and keeps the
port up to date. To set it up by hand, download the [latest release](https://github.com/Barthes-Recompiled/HP3-Recompiled/releases/latest),
put your disc's files or the .iso in a `game` folder beside the executables and start `hppoa_launcher.exe`
([port/README.txt](port/README.txt) has the details).

**This repository contains no game content.** To build it you need the files from your own copy of the Xbox disc
(USA for now; the European disc later). The build regenerates the recompiled code from your `default.xbe`.

## Building

See [port/DEVELOPING.md](port/DEVELOPING.md): MSYS2 (UCRT64) setup, regenerating the code from the XBE, building,
debugging tools and how the port works. The design and plans are in [docs/superpowers/](docs/superpowers/).

## Credits and licences

The port's own code is MIT licensed, © 2026 MiddleDucky and barthes_recompiled contributors ([LICENSE](LICENSE)). The parts
below keep their own licences and copyright.

**MiddleDucky** made the Philosopher's Stone and Chamber of Secrets Xbox ports this one is built from
([HP-PS-Xbox-Recomp](https://github.com/MiddleDucky/HP-PS-Xbox-Recomp),
[HP-COS-Xbox-Recomp](https://github.com/MiddleDucky/HP-COS-Xbox-Recomp)): the port layer, renderer adaptation, audio,
input, launcher, trainer, region retargeting and much of the toolkit work. MiddleDucky has also started a Prisoner of
Azkaban port of their own ([HP-POA-Xbox-Recomp](https://github.com/MiddleDucky/HP-POA-Xbox-Recomp)), and two fixes in
this toolkit come from their work on PoA: the factory AV region answer (PoA times its frames by the video standard)
and NV2A-style line drawing (PoA's shadow fade mask relies on it).

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
