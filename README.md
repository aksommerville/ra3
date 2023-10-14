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
- - [ ] Database
- - [ ] Launcher
- - [ ] HTTP server
- [ ] Menu
- [ ] Web
- [ ] Integrate emulators
- - [ ] akfceu
- - [ ] aksnes9x
- - [ ] akgambatte
- - [ ] akprosys
- - [ ] Stella
- - [ ] Genesis?
- - [ ] PC Engine?
- - [ ] Pico-8. No integration, but do get it running on the Pi with DRM.
- - [ ] Solarus, try again.
- [ ] Prepare collection
- - [ ] Full set of ROMs. Eliminate duplicates, faulty, and obscene.
- - [ ] Selections for each user.
- - [ ] "Andy's Top Picks".
