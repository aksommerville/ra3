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
- - [x] Input manager
- - - [ ] Default config for keyboards (when no config file present). See eh_inmgr_config.c
- - [ ] Deliver screencaps.
- - - [ ] Screencaps from OpenGL context. Need a native OpenGL client to test this.
- - [ ] Option to black out some portion of the framebuffer edge? I think Castlevania 2 could benefit from this.
- [ ] Backend
- - [ ] Check that we set O_CLOEXEC on all long-lived files; I bet we don't.
- - [ ] Real time framebuffer stream and input override -- support the GDEX use case, where there's an RA server on each play station, and admin on a laptop.
- - - [ ] Have game open its own server so web client can connect directly, don't pass thru a middleman.
- - [x] !!! Freeze trying to launch a game immediately after creating the new `stella` launcher. ...because I forgot "$FILE" in the command? YES
- - [x] Is query (pubtime) broken? Doesn't seem to have any effect.
- [ ] db: Add multiple files from fs by providing one path.
- [ ] db: Support for automatic updates. Record gittable directories?
- [ ] Menu
- [ ] Web
- - [ ] Admin: What is this for?
- - [ ] Now Playing: Flesh out with WebSocket.
- - [ ] Search results: Screencaps aren't maintaining aspect ratio.
- - [ ] Search results: Can we force cards to pack at the top? When there's 6 results in 4 columns, there's a gap between the rows.
- [ ] Integrate emulators
- - [x] akfceu
- - [x] Stella ...went with z26 instead
- - [x] aksnes9x
- - [x] akgambatte
- - - [x] Ensure ".sav" files go somewhere outside the roms dir.
- - [x] akprosys
- - [x] Super Cassette Vision?
- - - See MAME: src/main/epoch/scv.cpp. Can we tease it out of there? I definitely don't want all of MAME.
- - - Crunching to get v3 done and polished before Christmas 2023, so let's forget about SCV for the near future at least. -ak 2023-11-08
- - [x] N64? NO
- - [x] Genesis? NO
- - [x] PC Engine? NO
- - [ ] Pico-8. No integration, but do get it running on the Pi with DRM.
- - [ ] Solarus, try again.
- - [ ] Validate 4 player in all emulators
- [ ] Prepare collection
- - [ ] Drop records for everything on platforms we're not supporting yet, no sense keeping these in the mix.
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene. Populate metadata for everything we keep.
- - - Games initially with rating==0 (in pages): gameboy=81 snes=27 nes=20 atari2600=44 atari5200=8 atari7800=6 c64=186 genesis=114 n64=11 scv=3
- - - That's about 6000 games. But we're probably not going to support 5200, c64, n64, or genesis, and scv remains highly questionable.
- - - gameboy+snes+nes+atari2600: 2064 games. Still a ton.
- - [ ] Selections for each user.
- - [ ] "Andy's Top Picks".
- - [x] There's one game with platform unset, which is it?
