# Romassist HTTP Server

```
*    /api/**  REST API
WS   /ws/**   WebSocket
GET  /**      Static files
```

## REST and WebSocket API Overview

```
GET /api/meta/flags => array of 32 strings (constant)
GET /api/meta/platform?detail => [name...] (detail="id", default) or [{v,c}...] (detail="record")
GET /api/meta/author?detail => [name...] (detail="id", default) or [{v,c}...] (detail="record")
GET /api/meta/genre?detail => [name...] (detail="id", default) or [{v,c}...] (detail="record")
GET /api/meta/daterange => [lo,hi] (year only, as numbers)
GET /api/meta/all => ...

GET /api/game/count => integer
GET /api/game?index&count&detail => Game[]
GET /api/game?gameid&detail => Game
PUT /api/game?detail <= Game => Game
PATCH /api/game?detail <= Game => Game (must exist)
DELETE /api/game?gameid => nothing
GET /api/game/file?gameid => binary
PUT /api/game/file?dry-run&name&size&platform <= binary => Game

GET /api/comment/count => integer
GET /api/comment?index&count => Comment[]
GET /api/comment?gameid&time&k => Comment[] (time is minimum, k is exact; normal to omit time and k)
PUT /api/comment <= Comment => Comment
PATCH /api/comment <= Comment  => Comment (must exist)
DELETE /api/comment?gameid&time&k => nothing

GET /api/play/count => integer
GET /api/play?index&count => Play[]
GET /api/play?gameid&since => Play[]
PUT /api/play <= Play => Play (always new)
PATCH /api/play <= Play => Play (must exist. unusual. prefer POST /api/play/terminate)
POST /api/play/terminate?gameid => Play (must exist)
DELETE /api/play?gameid&time => nothing

GET /api/launcher/count => integer
GET /api/launcher?index&count => Launcher[]
GET /api/launcher?launcherid => Launcher
GET /api/launcher?gameid => Launcher (dynamically selects the appropriate one)
PUT /api/launcher <= Launcher => Launcher (always new)
PATCH /api/launcher <= Launcher => Launcher (must exist)
DELETE /api/launcher?launcherid => nothing

GET /api/upgrade/count => integer
GET /api/upgrade?index&count => Upgrade[]
GET /api/upgrade?upgradeid|gameid|launcherid|name => Upgrade
PUT /api/upgrade <= Upgrade => Upgrade (always new)
PATCH /api/upgrade <= Upgrade => Upgrade (must exist)
DELETE /api/upgrade?upgradeid => nothing
POST /api/upgrade?(upgradeid)&(before)&(dry-run) => ...

GET /api/listids => (string|number)[]
GET /api/list/count => integer
GET /api/list?index&count&detail => List[]
GET /api/list?listid&detail => List
GET /api/list?gameid&detail => List[] (those including gameid)
PUT /api/list?detail <= List => List (always new)
PATCH /api/list?detail <= List => List (must exist. if "games" present, they replace the whole array)
DELETE /api/list?listid => nothing
POST /api/list/add?listid&gameid&detail => List ; name also accepted as listid
POST /api/list/remove?listid&gameid&detail => List ; name also accepted as listid

GET /api/blob/all => [path...]
GET /api/blob?gameid&type => [path...]
GET /api/blob?path => raw content. (path) is the real local fs path; if you call from localhost, you could read it directly instead.
PUT /api/blob?gameid&type&sfx <= raw => path (new blob)
DELETE /api/blob?path => nothing

POST /api/query?text&list&platform&author&genre&flags&notflags&rating&pubtime&detail&limit&page&sort => Game[]

GET /api/histograms => ...

POST /api/launch?gameid => Play
POST /api/random?text&list&platform&author&genre&flags&notflags&rating&pubtime => Play
POST /api/terminate => nothing
POST /api/enable-public => int (TCP port)

POST /api/autoscreencap => report

GET /api/export => See below.

WS /ws/menu => Long running connection for menu clients.
WS /ws/game => Long running connection for game clients.
```

Some general rules:

- Request and response bodies are JSON, with a few obvious exceptions like blob content.
- Missing fields are equivalent to zero, empty string, empty array, etc.
- Integers are unsigned unless noted. (i think they're all unsigned...)
- Timestamps are ISO 8601. The database internally only records down to the minute.
- We automatically convert between strings, numbers, and booleans. Stated types are what we return.

## /api/meta

Constant-ish metadata from the database.

`GET /api/meta/flags` is literally constant; it returns a hard-coded list of strings for the flag names.

`platform`, `author`, and `genre` are generated dynamically on each request, based on values in the games table.
Use `detail="id"` or omit for a simple array of strings, or `detail="record"` to get `{v,c}` the name and count of each.

`daterange` returns the year of the lowest and highest pubtime in the database, array of two ints, not counting zeroes.
If no game has a date, we return the current year twice.

`GET /api/meta/all` to do all these things and return them together:

```
{
  flags: string[32]
  platform: string[]
  author: string[]
  genre: string[]
  daterange: int[2]
  listids: (string|int)[]
}
```

## /api/game

When `detail` is `"id"`, a Game is just an integer, the ID.

```
game {

// detail>=name:
  gameid: int
  name: string
  
// detail>=record:
  platform: string
  author: string
  genre: string
  flags: string; space-delimited list of flag names
  rating: int; conventionally 0..99, with 0 meaning unset
  pubtime: string
  path: string
  
// Everything below this point is ignored as input.
  
// detail>=comments:
  comments: [{
    time: string
    k: string
    v: string
  }...]
  
// detail>=plays:
  plays: [{
    time: string
    dur_m: string
  }...]
  
// detail>=lists:
  lists: [{
    listid: int
    name: string
  }...]
  
// detail>=blobs:
  blobs: string[]; paths
  
}
```

The special endpoint `GET /api/game/file?gameid` lets you download ROM files verbatim.
Always be mindful of platform; if "native", the returned executable is not likely to work on other machines.
That is very much Not My Problem :P

You can also `PUT /api/game/file` to upload a game.
Server will create a new copy of the file, even in the common case that client and server are the same machine.
This has a parameter `dry-run=true` to do all the decision-making but don't actually commit the change.
In that case, the returned `gameid` will be zero. You can supply `size=BYTES` instead of the request body for dry runs.
You must supply at least one of (`name`, `platform`).

## /api/comment

```
comment {
  gameid: int; READONLY
  time: string; READONLY
  k: string; READONLY
  v: string
}
```

`k` is expected to be a small C identifier from some well-known set.
TODO define that set.

## /api/play

```
play {
  gameid: int; READONLY
  time: string; READONLY
  dur_m: int
}
```

## /api/launcher

```
launcher {
  launcherid: int
  name: string; specific to this launcher, eg "akfceu"
  platform: string; for matching games, eg "nes"
  suffixes: string; comma-delimited list of path suffixes
  cmd: string; shell command. "$FILE" replaced with game's path.
  desc: string; commentary from user
}
```

## /api/upgrade

```
upgrade {
  upgradeid: int
  name: string; usually empty -- use the game or launcher's name
  displayName: string; READONLY
  desc: string; commentary
  gameid: int
  launcherid: int
  depend: int; upgradeid
  method: string; "git+make", and expect other in the future
  param: string; other parameters per (method). Directory for "git+make".
  checktime: string
  buildtime: string
  status: string; empty means ok, anything else is a description of the last failure
}
```

`POST /api/upgrade` to trigger upgrades.
This will not wait for the upgrades to complete. Only queues them for processing and reports what's been queued.
If any upgrades are currently pending, we respond 409 (Conflict) and do nothing.

No parameters to upgrade everything immediately.
- `upgradeid` to do just one.
- `before=YYYY-MM-DDTHH:MM` to do those whose last check is before the given time.
- `dry-run=1` to do nothing, only report back which upgrades would be performed.

Responds with a report of action taken:
```
{
  upgrades: {
    upgradeid
    displayName
    checktime
    buildtime
    status
  }[]
}
```

## /api/listids

Returns just the name (or ID if no name) of every list in the DB.
Intended to work like the `/api/meta` calls, for autocomplete and such.

## /api/list

```
list {
  listid: int
  name: string; Loose and mutable, but can be used for querying so try to keep it simple.
  desc: string
  sorted: boolean
  games: Game[]
}
```

## /api/blob

Listing blobs means iterating over directories, it's more expensive than most DB ops.

Blobs are identified by paths, which are the full path on the server's filesystem.
You are probably subject to this same filesystem, and can access these files directly.
I recommend using the HTTP API to list, create, or delete blobs, but for reading them definitely use the filesystem directly.

## /api/query

Search for games.

There are three different ways to search, which can be combined in one request:

- `text`: Loose text from name, basename, and comments.
- `list`: Restrict to contents of one list. (ID or exact name). If provided more than once, the lists are intersected.
- `platform`, `author`, `genre`: Exact match of game header strings. You can't search for empty strings.
- `flags`, `notflags`: Comma-delimited lists of flag names that must be 1 or 0.
- `rating=LO..HI`: OK to omit one endpoint, or a simple integer for exact matches (which is a bit weird?).
- `pubtime=START..END`: Or a single time, with precision implied by the format (eg "2023" includes "2023-10-15T12:45" but "2023-09" does not).

We don't provide a general query language, that would be overkill.
So some things like union-of-lists, we can't do.
Either make a broader query than needed and filter on your end, or add an HTTP endpoint and call `db_query_generic()`.
If you provide no criteria, every game matches.

After filtering, we can sort, paginate, and format to taste:

- `detail=record`: Same as Game calls.
- `limit=100`: Maximum results to return.
- `page=1`: One-based page index, if `limit` in play.
- `sort`: Order of results.

Sort is one of:
- none
- id
- name
- pubtime
- rating
- playtime
- playcount
- fullness
And may be prefixed with "-" to reverse.

The HTTP response will include headers `X-Page-Count` and `X-Total-Count` when paginated.

## /api/histograms

Return a report showing the entire collection sliced by various properties.

{
  platform: {v,c}[]
  author: {v,c}[]
  genre: {v,c}[]
  rating: {v,c}[] v numeric (exact rating)
  pubtime: {v,c}[] v numeric (year)
}

## /api/launch, /api/random, /api/terminate

Launches a game.
Any game in progress will be terminated first.
A new `play` record will be added and returned.

`/api/random` takes the same criteria parameters as `/api/query`, but instead of returning the results,
it picks a game at random from within them and launches it.
I expect to expose this behavior directly on frontends wherever the user can search. "Roll dice within this query".
`dry-run=1` to return results just like `/api/query` with extra headers `X-Page-Index:int` and `X-Game-Id:int`.

Or `/api/terminate` to stop whatever game is in progress and return to the menu.
The menu itself does not terminate on this, though it technically could.

## /api/shutdown

Quit the server or power down the machine (whichever the backend is configured to do).
An extra header is required, to illustrate the call's gravity: X-I-Know-What-Im-Doing: true

## /api/enable-public

Ask server to open its public TCP port, if it's configured to.
This is necessary on my Raspberry Pi at least, because the server starts running before ethernet is fully configured.
So the main server falls back to localhost-only, which is fine for everything except web access from a cell phone.
And the idea of not serving on INADDR_ANY until being told to, that feels right.

## /api/autoscreencap

Every game that doesn't have a "scap" blob yet, try to generate one.
Right now this only works for Pico-8, where an image is embedded in the ROM.
Would be cool in the future if we can launch a headless emulator, run for a few seconds, then grab the framebuffer.
But that's probably overkill.

Returns a JSON report describing what changed.

## /api/export

Dump the entire database in a more or less portable format.

| Param        | Desc |
|--------------|------|
| comments     | 0,[1] |
| plays        | 0,[1] |
| launchers    | [0],1 |
| upgrades     | [0],1 |
| lists        | 0,[1] |
| blobs        | 0,[1] |

Response:
```
{
  games: Table(
    gameid: int
    name: string
    path: string
    platform: string
    author: string
    genre: string
    flags: string[]
    rating: int
    pubtime: string
    comments?: Table(
      time: string,
      k: string,
      v: string,
    ) or zero if none,
    plays?: Table(
      time: string,
      dur: int, minutes,
    ) or zero if none,
  ),
  launchers?: Table(
    launcherid: int,
    name: string,
    platform: string,
    suffixes: string,
    cmd: string,
    desc: string,
  ),
  upgrades?: Table(
    upgradeid: int,
    name: string,
    desc: string,
    gameid: int,
    launcherid: int,
    depend: int (upgradeid),
    method: string,
    param: string,
    checktime: string,
    buildtime: string,
    status: string,
  ),
  lists?: Table(
    listid: int,
    name: string,
    desc: string,
    sorted: int,
    gameids: int[],
  ),
  blobs?: string[], basename only
}
```

`Table` is an array whose first member is an array of strings (keys).
Subsequent members are arrays of values only.

## WebSocket

Connect to `/ws/menu` or `/ws/game` depending on your role.
I'll document them here as which roles are expected on each side of each packet ("server" is the third role).
Packet bodies are JSON or binary. In all cases they must be self-describing.
JSON packets must have "id".
Binary packets... Make sure the intent is clear based on payload type.

There can be multiple MENU clients, or zero.
Typical usage on a console, the menu doesn't run while games are running.
On a PC, the menu is usually a web app that keeps running all times, and there could be more than one.

id=hello ANY=>ANY
  No further data. Just a nonsense packet to keep the connection alive or whatever.
  
id=game GAME=>SERVER SERVER=>MENU
  Identify game in progress. If it only contains (id), that means no game in progress.
  {id,name,gameid,platform,path,exename}
  
id=requestScreencap MENU=>SERVER SERVER=>GAME
  Ask the game to send us one screencap at its convenience.
  Game should respond with a binary packet containing a PNG file of the raw framebuffer.
  
Binary=PNG GAME=>SERVER SERVER=>MENU
  Any binary packet containing a PNG file is presumed to be a response to requestScreencap.
  Game can send these on its own initiative too; we should have a hotkey for "take screencap".
  When the server gets one of these without a recent request, it should save a blob and not forward to the menu.
  
id=pause MENU=>SERVER SERVER=>GAME
id=resume MENU=>SERVER SERVER=>GAME
id=step MENU=>SERVER SERVER=>GAME
  Hard pause or step by video frame.
  
id=comment GAME=>SERVER SERVER=>MENU
  Game should report scores if it knows how to.
  Server echoes these to all menus, just for awareness.
  Server is left to its own discretion whether to record the comment.
  {id,k,v}
  
id=http GAME=>SERVER MENU=>SERVER
  Cheap trick to allow HTTP requests over a WebSocket connection, for (typical) clients that don't want multiple sockets open.
  Requests are served immediately.
  Provide (path) and (query) separate.
  (query) and (headers) should be simple {key:value} objects.
  (body) must be a string. Use header "X-Body-Encoding: base64" for binary data. (there's probably a generic HTTP equivalent for that but I'm too lazy to search).
  All HTTP calls documented above should work, and should grok all future changes automatically; this abstraction is implemented generically.
  If you need to be certain which response is associated with which request, add a header "X-Correlation-Id", and server echoes it back.
  Long response bodies are no problem, but most other fields have static limits. (src/romassist/ra_websocket.c if you need to tweak).
  {id,method,path,query,headers,body}
  
id=httpresponse SERVER=>GAME SERVER=>MENU
  Every "http" packet results in exactly one "httpresponse" packet back.
  {id,status,message,headers,body}
  
id=upgrade SERVER=>MENU
  Status change relating to upgrades.
  (pending) are ones we haven't started yet.
  (running) without (text) or (status) means we've just started one.
  (text) is output from the current upgrade, should also have (running).
  (status) is the exit status of the current upgrade, should also have (running) and it's the last time you'll see it.
  {
    id,
    pending?: upgrade[],
    running?: upgrade,
    text?: string,
    status?: int,
  }
