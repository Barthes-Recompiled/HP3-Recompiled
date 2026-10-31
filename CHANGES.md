# Changes

## 0.5.0 Beta (2026-10-31): the first release

The first public build of the Prisoner of Azkaban port, a beta. It stands entirely on **MiddleDucky's**
Philosopher's Stone and Chamber of Secrets ports ([HP-PS-Xbox-Recomp](https://github.com/MiddleDucky/HP-PS-Xbox-Recomp),
[HP-COS-Xbox-Recomp](https://github.com/MiddleDucky/HP-COS-Xbox-Recomp)): the port layer, the renderer work, the
audio, the input, the launcher and much of the toolkit come from them. Thank you, MiddleDucky, wholeheartedly; none of
this exists without you.

### What works
- Boots to the title screen and its attract loop, and New Game plays through the Hogwarts Express into the first level.
- Saving and loading.
- **60 fps by default**, with the game running at the right speed (the game clock's 30 fps floor is lifted); 30 fps
  and higher caps are in the launcher and F10.
- **True 16:9:** the 3D view is widened, menus, loading screens and in-game menus keep their 4:3 shape, and cutscenes
  keep their original letterbox.
- The settings of the sibling ports in the launcher and F10: resolution up to 4K, render scale, MSAA, SMAA,
  anisotropic filtering, windowed / borderless / fullscreen, keyboard and mouse with rebinding, controller remapping,
  deadzones and vibration, and "Controller in background". F10 uses the sibling ports' shared row table,
  with left/right arrows, descriptions and a scrolling layout for 4:3 through 4K.
- **PC Settings in the pause menu:** Back on the pause menu opens a page of the port's settings (display, frame
  rate, anti-aliasing, controller, keyboard and mouse, cutscene bars), drawn over the paused game in its 4:3 menu
  area; a "Back  PC Settings" hint shows on the pause screen.
- Sound, including the narrator's line on the "I solemnly swear" screen.

### Known issues
- Only the North American (NTSC-U) disc works.
- Levels after the first have not been checked yet.
- No "Quit To Desktop" entry in the menus (use Alt+F4). The main menu has no settings page (use F10 there).
- Cutscene fast-forward and the start-up logo skip are visible but greyed out, marked
  "Not working in this game yet", in F10 and the launcher.
- Button prompts are always Xbox's; PlayStation and Switch pads work through Steam Input or DS4Windows.
