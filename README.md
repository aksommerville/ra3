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
- - [ ] Option to black out some portion of the framebuffer edge? I think Castlevania 2 could benefit from this.
- [ ] Backend
- - [ ] Real time framebuffer stream and input override -- support the GDEX use case, where there's an RA server on each play station, and admin on a laptop.
- - - [ ] Have game open its own server so web client can connect directly, don't pass thru a middleman.
- [ ] db: Add multiple files from fs by providing one path.
- [ ] Upgrade method for db content.
- [ ] Wrapper script to relaunch backend on specific exit statuses, for when it updates itself?
- [ ] menu: "corrupted size vs. prev_size" on startup, no idea why. random. Has to be either initial HTTP responses or the home/carousel/menubar/gamedetails UI.
- - 2023-11-26T09:21: Not necessarily the same problem (no error message), but got a failure hundreds of runs after adding startup logs.
- - - src/menu/mn_main.c:28 Reached end of mn_update, first frame. Exit status 0.
- - - ^ That exit status might be a lie; we weren't checking WIFEXITED.
- - - The 6 expected HTTP calls did go out, and succeeded, same as normal cases.
- - 2023-11-27T16:10: double free or corruption (out) ; not during startup. twice within 10 or so launches. probably audio related ...fixed a cheapsynth allocation thing, and it seems ok now.
- - 2023-11-28T16:25: Another segfault submitting search entry. I bet it happens during processing of the incoming search results.
- - - Spamming the search box can reproduce it, apparently, but it might take a hundred tries.
- - - ...grr 400 tries and nothing
- - - With more extensive logging, somehow it's happening a lot more! Only takes like 5 tries each time now.
- - - Not during gamedetails update or carousel update. We receive the response, tick the gui, then... unknown
- - - Definitely during gui_update. First root update after AUX1 went true.
- - - gui_widget_keyboard was modifying internal state after its callback, and the callback deletes it. easy fix. but that can't be the same bug as the startup segfaults.
- - - I expect we're still exposed to that class of problem elsewhere. Widget fires a callback that destroys it.
- - [ ] Observed empty search results at launch, when a valid 30-ish-game query was present.
- - [ ] Don't show "args" comments in gamedetails.
- [ ] Web
- - [ ] Now Playing: Flesh out with WebSocket.
- - [ ] Search results: Screencaps aren't maintaining aspect ratio.
- - [ ] Search results: Can we force cards to pack at the top? When there's 6 results in 4 columns, there's a gap between the rows.
- - [ ] Make all views mobile-friendly, today they are very not.
- - [ ] Don't show "args" comments in search results. While you're at it, show multiple "text" comments.
- [ ] Integrate emulators
- - [ ] Validate 4 player in all emulators
- - [ ] akz26: Review inputs. I think I'm missing some of the console switches.
- - [ ] akz26: Can we emulate paddles with the Atari Modern Joystick's twist feature? (i mean, that is what it's designed for...)
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
