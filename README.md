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
- [ ] Upload screencap via web app. Or provide URL? I'm thinking if you go search for box art somewhere else.
- - Actually this can be low priority. Pico-8 gets them automatically, and the emulators you can snap during play. Anything else is a bit exotic.
- [ ] mn_widget_edit/mn_widget_addgame: See comments, need some new support to facilitate popping up edit after adding a file.
- [ ] GET /api/blob/all is only returning buckets 0 and 1200, but there are dozens more.
- [ ] menu: Feedback from upgrade. Also, I'm not seeing git/make output in the server log, that would be nice.
- [ ] Integrate emulators
- - [ ] Validate 4 player in all emulators
- [ ] Prepare collection
- - [ ] See rating histogram, try to flatten out the multiple-of-5 spikes.
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene. Populate metadata for everything we keep.
- - - Games initially with rating==0 (in pages): gameboy=81 snes=27 nes=20 atari2600=44 atari5200=8 atari7800=6 c64=186 genesis=114 n64=11 scv=3
- - - That's about 6000 games. But we're probably not going to support 5200, c64, n64, or genesis, and scv remains highly questionable.
- - - gameboy+snes+nes+atari2600: 2064 games. Still a ton.
- - - 2023-11-26: 1676 remaining
- - - 2023-12-02: 1558 ...1513
- [x] Concatenate "args" comments
- [x] "env" comments ...these would already work by default, "$COMMENT:" is generic.
- [ ] Emuhost: Add an overscan option, you never know.
- [x] emuhost.cfg: Not saving video-device?
- [x] Menu: audio-device should populate in UI.
- [ ] Prepare the Pi.
- - [ ] ALSA doesn't want to work over HDMI! Change everything to Pulse, confirmed it does work.
- - [ ] Opening server fails if no network! Must fall back to localhost.
- - [ ] Write a shell script to clone and build all the repos and then migrate the db.
- - [ ] Ensure keyboards behave sanely. ...as is they do not
- - [ ] systemd
- - [ ] Verify poweroff. We can sudo it without a password, at least at the command line.
- - [ ] Pico-8 is picky about multiple joysticks, it picks player1 and player2 on its own. Can we mitigate that somehow?
- - [ ] Solarus
- - [ ] Boot time is almost exactly 30 seconds. That's acceptable but can we improve it?
- - [ ] Confirm we don't block for the network on startup.
- - [ ] Understand wi-fi. Will it try to configure and connect itself automatically? Should we configure that somewhere?
- - [ ] We are listening on INADDR_ANY. Add at least some kind of protection against remote-network access. We should only allow clients on the same, unroutable, network.
- - - The home router should take care of that for us, but let's do something at least.
- - [ ] Is it possible to not show the terminal as the frontend app switches?
- - [ ] akz26 freeze at quit. Last output: /home/kiddo/proj/akz26/out/akz26: Normal exit. 714 frames in 12 s, 0 faults, video rate 60.087 Hz, CPU usage 0.252
- - - Got similar out of akfceu. Hot Slots
- - - Same from ctm once: ctm: Average frame rate 60.04 Hz.
- - - [ ] Check PulseAudio cleanup, maybe it blocks for the I/O thread or something. Pulse does remain in top during the freeze, and CPU doesn't spike or anything.
- [ ] A missing game shouldn't break list processing. db_list_gamev_populate, we abort if one is missing. tricky...
- [ ] If the menu's connection to Romassist gets broken, it's impossible to quit or shut down. Should we do something about this?
