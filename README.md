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
- - [ ] Config: Use a file that menu can edit.
- - [x] Video drivers
- - [x] Audio drivers
- - [x] Input drivers
- - [x] Input manager
- - - [ ] Default config for keyboards (when no config file present). See eh_inmgr_config.c
- - - [x] Don't let system keyboard occupy a player slot.
- - - [ ] Wire up stateless actions. See eh_cb_digested_event
- - [x] Renderer: Client supplies a framebuffer and drivers supply an OpenGL context. Emuhost does the rest, it's involved.
- - [ ] Audio live resampling: We can't force clients to use the hardware sample rate or format.
- - [ ] Communicate with backend.
- [ ] Backend
- - [x] WebSocket server
- - [x] Screencapping -- Need these before I start bulk review.
- - [ ] Check that we set O_CLOEXEC on all long-lived files; I bet we don't.
- - [x] Use Pico-8 ROMs as screencaps somehow.
- - [ ] Real time framebuffer stream and input override -- support the GDEX use case, where there's an RA server on each play station, and admin on a laptop.
- - [ ] http: Add a "fakewebsocket" upgrade option, so native clients don't need to bugger around with proper WebSocket framing.
- [ ] db: Add multiple files from fs by providing one path.
- [ ] db: Support for automatic updates. Record gittable directories?
- [ ] Menu
- [ ] Web
- - [ ] Admin: What is this for?
- - [ ] Now Playing: Flesh out with WebSocket.
- - [ ] Search results: Screencaps aren't maintaining aspect ratio.
- - [ ] Search results: Can we force cards to pack at the top? When there's 6 results in 4 columns, there's a gap between the rows.
- [ ] Integrate emulators
- - [ ] akfceu
- - [ ] aksnes9x
- - [ ] akgambatte
- - - [ ] Ensure ".sav" files go somewhere outside the roms dir.
- - [ ] akprosys
- - [ ] Stella
- - [ ] Super Cassette Vision?
- - [ ] N64?
- - [ ] Genesis?
- - [ ] PC Engine?
- - [ ] Pico-8. No integration, but do get it running on the Pi with DRM.
- - [ ] Solarus, try again.
- [ ] Prepare collection
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene. Populate metadata for everything we keep.
- - - Games initially with rating==0 (in pages): gameboy=81 snes=27 nes=20 atari2600=44 atari5200=8 atari7800=6 c64=186 genesis=114 n64=11 scv=3
- - - That's about 6000 games. But we're probably not going to support 5200, c64, n64, or genesis, and scv remains highly questionable.
- - - gameboy+snes+nes+atari2600: 2064 games. Still a ton.
- - [ ] Selections for each user.
- - [ ] "Andy's Top Picks".
