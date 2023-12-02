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

- [ ] Backend
- - [ ] Real time framebuffer stream and input override -- support the GDEX use case, where there's an RA server on each play station, and admin on a laptop.
- - - [ ] Have game open its own server so web client can connect directly, don't pass thru a middleman.
- [ ] Upgrade method for db content.
- [ ] menu: Feedback from upgrade. Also, I'm not seeing git/make output in the server log, that would be nice.
- [x] menu: "corrupted size vs. prev_size" on startup, no idea why. random. Has to be either initial HTTP responses or the home/carousel/menubar/gamedetails UI.
- - 2023-11-26T09:21: Not necessarily the same problem (no error message), but got a failure hundreds of runs after adding startup logs.
- - - src/menu/mn_main.c:28 Reached end of mn_update, first frame. Exit status 0.
- - - ^ That exit status might be a lie; we weren't checking WIFEXITED.
- - - The 6 expected HTTP calls did go out, and succeeded, same as normal cases.
- - [x] Observed empty search results at launch, when a valid 30-ish-game query was present.
- - 2023-12-01: Been a few days and hundreds of launches, and I'm still not seeing these. Remove the logs and assume it was a fluke, or fixed by accident since.
- [ ] Integrate emulators
- - [ ] Validate 4 player in all emulators
- [ ] Prepare collection
- - [ ] See rating histogram, try to flatten out the multiple-of-5 spikes.
- - [x] Review all native games. I know we have Bonnie's Bakery here, and pretty sure that won't fly on ARM.
- - - fwiw BB doesn't launch here either, is something broken? ...yes a space in the path. renamed the file, kept the bug
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene. Populate metadata for everything we keep.
- - - Games initially with rating==0 (in pages): gameboy=81 snes=27 nes=20 atari2600=44 atari5200=8 atari7800=6 c64=186 genesis=114 n64=11 scv=3
- - - That's about 6000 games. But we're probably not going to support 5200, c64, n64, or genesis, and scv remains highly questionable.
- - - gameboy+snes+nes+atari2600: 2064 games. Still a ton.
- - - 2023-11-26: 1676 remaining
- [ ] Prepare the Pi.
- - [x] Migrate data.
- - [ ] Ensure keyboards behave sanely.
- - [ ] Map every joystick I've got.
- - [ ] systemd
- - [ ] Verify poweroff. We can sudo it without a password, at least at the command line.
- - [ ] Pico-8
- - [ ] Solarus
- - [ ] Boot time is almost exactly 30 seconds. That's acceptable but can we improve it?
- - [ ] Confirm we don't block for the network on startup.
- - [ ] Understand wi-fi. Will it try to configure and connect itself automatically? Should we configure that somewhere?
- - [ ] We are listening on INADDR_ANY. Add at least some kind of protection against remote-network access. We should only allow clients on the same, unroutable, network.
- - - The home router should take care of that for us, but let's do something at least.
- - [x] ctm: Must take DRM device path from argv
- - [x] Same for Full Moon
- - [x] chetyorska: "Failed to initialize video." Unclear which driver it's trying.
- - - Video driver is managed in rabbit, not in chetyorska. rabbit... what was i thinking.
- - [x] Full Moon: Music screwy. Especially noticeable in Toil and Trouble. stdsyn, pulse, 44100, 1
- - - Force alsa and minsyn and it goes away. Must be some signed-float ARM thing? Just use minsyn.
- - [x] Full Moon: Second launch stalls at startup. ...fluke? I'm trying again and it works. ...call it a fluke
- - [ ] Is it possible to not show the terminal as the frontend app switches?
- - [x] sitter2009: No audio suddenly. I swear it was working yesterday.
- - [x] plundersquad: Same
- - [x] ivand: Update DRM, dumb buffers are just not going to work. Follow example in pokorc.
- - [x] lilsitter: ''
- - [x] git needs username and password to pull. it must not ask!
- - - Some of these repos are private. Easy, just make them all public.
- - [ ] akz26 freeze at quit. Last output: /home/kiddo/proj/akz26/out/akz26: Normal exit. 714 frames in 12 s, 0 faults, video rate 60.087 Hz, CPU usage 0.252
- [ ] ra_migrate: When taking games from a list, confirm that they exist. We're getting bit by this, due to non-upgrade native games (eg Bonnie's Bakery)
- [ ] ra_migrate: Do at least some dumb correction attempt for "args" comments, launcher cmd, and upgrade param. Look for paths that need to change due to username.
- [ ] A missing game shouldn't break list processing. db_list_gamev_populate, we abort if one is missing. tricky...
- [ ] If the menu's connection to Romassist gets broken, it's impossible to quit or shut down. Should we do something about this?
- [ ] web: ListsUi fetches with "detail=id" then tries to display the names. "undefined: undefined"
