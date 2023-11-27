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
