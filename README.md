# Romassist v3

Several components:
- Library `emuhost` for emulators to consume, so they all behave the same.
- Backend app with database, HTTP server, and process launcher intended to run as a systemd service.
- Frontend app for interacting with that.
- Web app, does the same as frontend but via HTTP.

The new emuhost will not be compatible with emuhost v2. I'm reworking that interface from scratch.

Ditto for all the HTTP interfaces.

## TODO

- [ ] Emuhost
- - [ ] Config
- - [ ] Video drivers
- - [ ] Audio drivers
- - [ ] Input drivers
- [ ] Backend
- - [ ] WebSocket server
- - [ ] Check that we set O_CLOEXEC on all long-lived files; I bet we don't.
- - [x] Use Pico-8 ROMs as screencaps somehow.
- [ ] db: Add multiple files from fs by providing one path.
- [ ] db: Support for automatic updates. Record gittable directories?
- [ ] Menu
- [ ] Web
- - [ ] Admin: What is this for?
- - [ ] Now Playing: Flesh out with WebSocket.
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
- - [ ] Selections for each user.
- - [ ] "Andy's Top Picks".
