# Romassist data guide.

## Launchers

Each `launcher` record is a means of launching one or more games.
Typically that means an emulator.

There are two special launchers:
- "never" to forcibly prevent a game from launching.
- "native" where the game itself is an executable, not a ROM file.

Launchers' `cmd` string is split on spaces and passed to `execvp()`.
It is *not* a shell command.

Any token in `cmd` beginning with `$` is a variable:
- `$FILE` for the game's path.
- `$COMMENT:foo` for the value of the first comment of type "foo" for this game, or empty.
- Anything else is an error; the launcher won't work.

Native launcher should contain `$COMMENT:args` for extra text to add to the command.
That way the game's path can still be the actual executable path, no extra cruft.

## Upgrades

Each `upgrade` record is a game or launcher that can be automatically updated.

`method` should be `git+make`. I'll probably add other methods in the future.
There's a field `param` to provide additional data per method.
For `git+make`, `param` is the directory in which to do both `git pull` and `make`.

Set `depend` to the id of an upgrade that should be run first.
There's an upgrade record for Romassist itself, which includes Emuhost, and all emulators should depend on it.
We don't prevent upgrade records from creating dependency cycles.
Nothing should break if that happens, but be mindful if you're doing anything recursive with (depend).

Deleting a game or launcher deletes any upgrade associated with it.

Deleting an upgrade zeroes `depend` on other upgrades but does not delete them.
