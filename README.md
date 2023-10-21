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
- - [x] Database
- - [ ] Launcher
- - [x] HTTP server
- - [ ] WebSocket server
- [ ] db: Must cache blob list, at least for large game queries. (otherwise each file will re-read the blob directories from scratch).
- [ ] db: Add multiple files from fs by providing one path.
- [ ] db: Support for automatic updates. Record gittable directories?
- [ ] db: Pick a random game from within the query.
- [ ] Menu
- [ ] Web
- - [x] Straight DB view
- - [ ] Search
- - [ ] Game details
- - [ ] Lists
- - [ ] Admin
- - [ ] Now Playing
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
- - [x] Bulk import, include all ROMs, and metadata from ra2 if available.
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene.
- - [ ] Selections for each user.
- - [ ] "Andy's Top Picks".
