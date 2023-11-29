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

- [x] Have emuhost supply LDFLAGS. Some like pkg-config script.
- [x] sitter2009: Can't build imgcvt without libSDL. Need to rewrite it. ...actually no need to rewrite, just get the correct O files.
- [ ] Backend
- - [ ] Real time framebuffer stream and input override -- support the GDEX use case, where there's an RA server on each play station, and admin on a laptop.
- - - [ ] Have game open its own server so web client can connect directly, don't pass thru a middleman.
- [ ] Upgrade method for db content.
- [ ] Migration aid for two running instances across the network.
- [ ] menu: "corrupted size vs. prev_size" on startup, no idea why. random. Has to be either initial HTTP responses or the home/carousel/menubar/gamedetails UI.
- - 2023-11-26T09:21: Not necessarily the same problem (no error message), but got a failure hundreds of runs after adding startup logs.
- - - src/menu/mn_main.c:28 Reached end of mn_update, first frame. Exit status 0.
- - - ^ That exit status might be a lie; we weren't checking WIFEXITED.
- - - The 6 expected HTTP calls did go out, and succeeded, same as normal cases.
- - [ ] Observed empty search results at launch, when a valid 30-ish-game query was present.
- [x] I think at db_gc we are marking vacant strings as "to remove". Harmless but it impacts logging.
- [ ] Integrate emulators
- - [ ] Validate 4 player in all emulators
- - [ ] akz26: Review inputs. I think I'm missing some of the console switches.
- - [ ] akz26: Can we emulate paddles with the Atari Modern Joystick's twist feature? (i mean, that is what it's designed for...)
- - [x] akgambatte: Can we attenuate audio a bit? It's much louder than the other emulators. Reduced dramatically. (if it's quieter than others now, fine, it's also the crappiest!)
- [ ] Prepare collection
- - [ ] See rating histogram, try to flatten out the multiple-of-5 spikes.
- - [ ] Review all native games. I know we have Bonnie's Bakery here, and pretty sure that won't fly on ARM.
- - - fwiw BB doesn't launch here either, is something broken?
- - [ ] Review ROMs. Eliminate duplicates, faulty, and obscene. Populate metadata for everything we keep.
- - - Games initially with rating==0 (in pages): gameboy=81 snes=27 nes=20 atari2600=44 atari5200=8 atari7800=6 c64=186 genesis=114 n64=11 scv=3
- - - That's about 6000 games. But we're probably not going to support 5200, c64, n64, or genesis, and scv remains highly questionable.
- - - gameboy+snes+nes+atari2600: 2064 games. Still a ton.
- - - 2023-11-26: 1676 remaining
- [ ] Prepare the Pi.
- - [ ] Ensure keyboards behave sanely.
- - [ ] Ensure Fast Forward and Screencap are *not* mapped on mom and dad's machine.
- - [ ] systemd
- - [ ] Verify poweroff
- - [ ] Build all the uncontroversial software.
- - [ ] Pico-8
- - [ ] Solarus
- [ ] If the menu's connection to Romassist gets broken, it's impossible to quit or shut down. Should we do something about this?
