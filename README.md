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
- - - [x] Default config for keyboards (when no config file present). See eh_inmgr_config.c
- - [x] Deliver screencaps.
- - - [x] Screencaps from OpenGL context. Need a native OpenGL client to test this.
- - [ ] Option to black out some portion of the framebuffer edge? I think Castlevania 2 could benefit from this.
- [ ] Backend
- - [ ] Check that we set O_CLOEXEC on all long-lived files; I bet we don't.
- - [ ] Real time framebuffer stream and input override -- support the GDEX use case, where there's an RA server on each play station, and admin on a laptop.
- - - [ ] Have game open its own server so web client can connect directly, don't pass thru a middleman.
- - [ ] Protect against immediate failure from the menu.
- [ ] db: Add multiple files from fs by providing one path.
- [ ] db: Support for automatic updates. Record gittable directories?
- [ ] Menu
- - [x] Hi-res text is painful... bring in freetype
- - - ...freetype is fucking ridiculous. Do it with bitmaps.
- - [x] Launch games.
- - [ ] Edit details for focussed game.
- - [x] Scroll to next page.
- - [x] Persist search.
- - [x] Search options: List (mainly), rating, flags, text...
- - - [x] rating, date
- - - [x] text
- - - [x] Popup menus: Highlight current selection when opening
- - [ ] Admin menu.
- - [ ] Auto-upgrade.
- - [ ] Sound effects.
- - [ ] Background music? Could be helpful for the user setting levels as she starts up.
- - [ ] gui render helpers, must rewrite with GL 2
- [ ] Web
- - [ ] Admin: What is this for?
- - [ ] Now Playing: Flesh out with WebSocket.
- - [ ] Search results: Screencaps aren't maintaining aspect ratio.
- - [ ] Search results: Can we force cards to pack at the top? When there's 6 results in 4 columns, there's a gap between the rows.
- [ ] Integrate emulators
- - [ ] Pico-8. No integration, but do get it running on the Pi with DRM.
- - [ ] Solarus, try again.
- - [ ] Validate 4 player in all emulators
- [ ] Prepare collection
- - [x] Drop records for everything on platforms we're not supporting yet, no sense keeping these in the mix.
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene. Populate metadata for everything we keep.
- - - Games initially with rating==0 (in pages): gameboy=81 snes=27 nes=20 atari2600=44 atari5200=8 atari7800=6 c64=186 genesis=114 n64=11 scv=3
- - - That's about 6000 games. But we're probably not going to support 5200, c64, n64, or genesis, and scv remains highly questionable.
- - - gameboy+snes+nes+atari2600: 2064 games. Still a ton.
- - [ ] Selections for each user.
- - [ ] "Andy's Top Picks".
