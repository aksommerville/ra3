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
- - Go big. I want an interactive process via the GUI, where you enter the remote IP address, then one by one resolve discrepancies.
- - etc/doc/20240906-upgrade-notes.txt
- [ ] Upload screencap via web app. Or provide URL? I'm thinking if you go search for box art somewhere else.
- - Actually this can be low priority. Pico-8 gets them automatically, and the emulators you can snap during play. Anything else is a bit exotic.
- - ^ Not sure I agree with this. Just installed some new games on the Pi and ended up having to scp the screencaps over. Needs a fix.
- [ ] mn_widget_edit/mn_widget_addgame: See comments, need some new support to facilitate popping up edit after adding a file.
- [ ] GET /api/blob/all is only returning buckets 0 and 1200, but there are dozens more.
- [ ] menu: Feedback from upgrade. Also, I'm not seeing git/make output in the server log, that would be nice.
- [ ] Integrate emulators
- - [ ] Validate 4 player in all emulators
- [ ] Prepare collection
- - [ ] More lists. I'd like a list of like "the best of every style", all the archetype games.
- [ ] Emuhost: Add an overscan option, you never know.
- [ ] Prepare the Pi.
- - [ ] Solarus
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
- [ ] Find games that could use high-score persistence. (IDs here are on my Nuc)
- - [ ] 2758: Super Mario Duck Track
- [ ] When I play lots of games in a row, launching from web, it tends to forget the fullscreen setting. Why?
- [ ] More platforms
- - [ ] Genesis
- - [ ] C64
- - [ ] DOS?
- - [ ] Atari 5200?
- - [ ] Atari 400?
- - [ ] Super Cassette Vision? Give it another look.
- - [ ] PC Engine?
- - N64, PS1, and newer are out of scope. ROMs too big, and I don't want to support analogue input.
- [x] Tweaks in advance of GDEX 2025:
- - [x] Kiosk mode: Repair labels when switching back to normal mode.
- - [x] Enter kiosk mode via menu.
- - [x] Is B to edit enabled in kiosk mode? I don't remember disabling it. Definitely want it disabled! ...it is enabled
- - [x] Reduce visible content when in kiosk mode. I think just title, genre, pubtime, comment? Plus indication of player count.
- [ ] Web List editor is allowing me to add the same game more than once.
- [ ] `mn_widget_menubar.c`: Toast when entering kiosk. Need such a thing as toasts.
