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
- [ ] "Add one file" via web app, and confirm it works from a phone.
- [ ] Upload screencap via web app. Or provide URL? I'm thinking if you go search for box art somewhere else.
- [ ] "Add one file" via menu, from a thumb drive.
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
- [ ] Prepare the Pi.
- - [ ] Write a shell script to clone and build all the repos and then migrate the db.
- - [ ] Ensure keyboards behave sanely.
- - [x] Map every joystick I've got.
- - - [x] See ~/.lexaloffle/pico-8/sdl_controllers.txt. Joystick mapping but format is not clear.
- - - There's an SDL tool for it.
- - - Abandoned hope of mapping all the controllers, too many of my games with their own wacky opinions about config. Got the two that matter on all apps.
- - [ ] systemd
- - [ ] Verify poweroff. We can sudo it without a password, at least at the command line.
- - [x] Pico-8. Downloaded their Pi build, try it out...
- - - Works, easy peasy, didn't even need a custom SDL build.
- - - [x] Except no audio.
- - - - SDL_AUDIODRIVER=alsa SDL_AUDIO_DEVICE_NAME=hw:0,0 AUDIODEV=hw:0,0 SDL_PATH_DSP=hw:0,0 ./pico8_dyn -run ~/rom/pico8/celeste.p8.png 
- - - - Tried all three devices and none works.
- - - Custom build of SDL, and we'll need to adjust the environment at load:
- - - - LD_PRELOAD=/home/kiddo/pkg/pico-8/libSDL2-2.0.so.0 SDL_AUDIODRIVER=alsa AUDIODEV=hw:0,0 /home/kiddo/pkg/pico-8/pico8_dyn -run ~/rom/pico8/celeste.p8.png
- - - - [x] Smarten up ra_process to identify and apply environment declarations in a command.
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
- [x] Plunder Squad on Pi, I can enter the input configurator with Atari Stick but it doesn't pick up any button presses???
- - ...was due to the rotation controller. Not going to open that can of worms, just configure it carefully.
- [ ] Buy mom and dad licenses for every software where that's an option.
- - [ ] Pico-8
- - [ ] Hughson: Witch N Wiz
- - [ ] Morphcat: Space Gulls, Micro Mages
- - [ ] Search for others

Input mapping
EH: Use live tool in menu.
P8: Use SDL's "controllermap". Then one adjustment: "guide"=>"start", make heart open the menu
SIT9, edit config file, don't trust the auto-configurator. ~/.sitter/sitter.cfg
Also, sitter2009 under SDL gets the dpad as a hat, and can't handle hats. Not going to worry about that just now.
TTAQ: ~/.ttaq/input
CTM: ~/proj/ctm/ctm-data/input.cfg, and it dumps guessed config, thank you good app. Must explicitly "unused" buttons to disable them.
PS: Live tool in app
CHET: ~/proj/chetyorska/etc/input.cfg
FM: Live tool in app

joymap-Microsoft X-Box 360 pad=:a0 -10000 10000 pxleft 0x0 pxright:a0 -10000 10000 mnleft 0x0 mnright:a1 -10000 10000 0x0 0x0 pxduck:a1 -10000 10000 mnup 0x0 mndown:0 mnok:1 pxpickup:1 pxtoss:1 mncancel:2 pxpickup:2 pxtoss:2 mncancel:7 menu
joymap-Microsoft X-Box pad v2 (US)=:a0 -10000 10000 mnleft 0x0 mnright:a1 -10000 10000 mnup 0x0 mndown:a0 -10000 10000 pxleft 0x0 pxright:a1 -10000 10000 0x0 0x0 pxduck:0 mnok:3 mncancel:2 menu:0 pxjump:3 pxpickup:3 pxtoss

Evercade SIT9 on Nuc: Not distinguishable from Xbox360, and we don't read hats.
AtariStick, same problem.
PowerA SIT9 on Nuc: Holding right all the way makes it go left, what the heck.
Shouldn't be a problem in the DRM build so ignore it.

!!! deleted ttaq config by accident during ElCheapo. Everything before that is suspect again.
BEWARE: The file gets copied from ttaq/etc/input

--- on nuc ---
DEVICE     EH P8 SIT9 TTAQ CTM PS CHET FM
Xbox360    x  x  x    ?    x   x  x    x
Xbox       x  x  x    ?    x   x  x    x
Evercade   x  x  !!!  ?    x   x  x    x
PowerA     x  x  x    ?    x   x  x    x
ElCheapo   x  x  x    x    x   x  x    x
8bdPro2    x  x  x    x    x   x  x    x
AtariStick x  x  !!!  x    x   x  x    x
AtariPad
Zelda
8bdSnes
8bdNes

--- on pi ---
DEVICE     EH P8 SIT9 TTAQ CTM PS CHET FM
Xbox360
Xbox
Evercade   x  x  x    x    x   x  x    x
PowerA
ElCheapo
8bdPro2
AtariStick x  x  x    x    x   x  x    x
AtariPad
Zelda
8bdSnes
8bdNes

LD_PRELOAD=~/proj/thirdparty/SDL/build/.libs/libSDL2-2.0.so.0 ~/pkg/pico-8/pico8_dyn -run ~/rom/pico8/celeste.p8.png

