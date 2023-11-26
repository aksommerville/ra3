# Romassist v3

Several components:
- Library `emuhost` for emulators to consume, so they all behave the same.
- Backend app with database, HTTP server, and process launcher intended to run as a systemd service.
- Frontend app for interacting with that.
- Web app, does the same as frontend but via HTTP.

The new emuhost will not be compatible with emuhost v2. I'm reworking that interface from scratch.

Ditto for all the HTTP interfaces.

Old emulators are of course still compatible generically, as long we don't serve on port 6502.

## TODO

- [ ] Emuhost
- - [ ] Option to black out some portion of the framebuffer edge? I think Castlevania 2 could benefit from this.
- - [ ] Sometimes it launches in a window when I've asked for fullscreen.
- [ ] Backend
- - [ ] Real time framebuffer stream and input override -- support the GDEX use case, where there's an RA server on each play station, and admin on a laptop.
- - - [ ] Have game open its own server so web client can connect directly, don't pass thru a middleman.
- - [ ] Protect against immediate failure from the menu.
- [ ] db: Add multiple files from fs by providing one path.
- [ ] db: Support for automatic updates. Record gittable directories?
- [ ] Menu
- - [ ] Random launch (Press R2). Will need "dry-run" from the backend so we can capture the gameid and page index.
- - - Don't actually launch. Just use the random-launch logic, and jump to that game in UI.
- - [ ] Admin menu.
- - - [ ] Audio: UI ready-ish. Needs emuhost support.
- - [ ] Editing year, new value didn't populate in the year modal
- - [ ] entry: Submit on AUX1
- - [ ] Auto-upgrade.
- - [ ] Sound effects.
- - [ ] Background music? Could be helpful for the user setting levels as she starts up.
- - [ ] gui render helpers, must rewrite with GL 2
- - [ ] "corrupted size vs. prev_size" on startup, no idea why. random. Has to be either initial HTTP responses or the home/carousel/menubar/gamedetails UI.
- - - 2023-11-26T09:21: Not necessarily the same problem (no error message), but got a failure hundreds of runs after adding startup logs.
- - - - src/menu/mn_main.c:28 Reached end of mn_update, first frame. Exit status 0.
- - - - ^ That exit status might be a lie; we weren't checking WIFEXITED.
- - - - The 6 expected HTTP calls did go out, and succeeded, same as normal cases.
- - [ ] Observed empty search results at launch, when a valid 30-ish-game query was present.
- [ ] Web
- - [ ] Admin: What is this for?
- - [ ] Now Playing: Flesh out with WebSocket.
- - [ ] Search results: Screencaps aren't maintaining aspect ratio.
- - [ ] Search results: Can we force cards to pack at the top? When there's 6 results in 4 columns, there's a gap between the rows.
- - [ ] Make all views mobile-friendly, today they are very not.
- [ ] Integrate emulators
- - [ ] Validate 4 player in all emulators
- - [ ] akz26: Review inputs. I think I'm missing some of the console switches.
- - [ ] akz26: Can we emulate paddles with the Atari Modern Joystick's twist feature? (i mean, that is what it's designed for...)
- [ ] Prepare collection
- - [x] Comprater
- - [ ] Bring in TTAQ, and look for other of my games.
- - [ ] Review all native games. I know we have Bonnie's Bakery here, and pretty sure that won't fly on ARM.
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene. Populate metadata for everything we keep.
- - - Games initially with rating==0 (in pages): gameboy=81 snes=27 nes=20 atari2600=44 atari5200=8 atari7800=6 c64=186 genesis=114 n64=11 scv=3
- - - That's about 6000 games. But we're probably not going to support 5200, c64, n64, or genesis, and scv remains highly questionable.
- - - gameboy+snes+nes+atari2600: 2064 games. Still a ton.
- - [x] Genre underused: Flyswatter, Trivia, Strategy.
- [ ] Would it be crazy to bake the menu into the backend app? It's not urgent but think this over some time.
- [ ] Prepare the Pi.
- - [ ] Ensure keyboards behave sanely.
- - [ ] Ensure Fast Forward and Screencap are *not* mapped on mom and dad's machine.
- - [ ] systemd
- - [ ] Verify poweroff
- - [ ] Build all the uncontroversial software.
- - [ ] Pico-8
- - [ ] Solarus
