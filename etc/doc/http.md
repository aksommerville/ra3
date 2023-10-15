# Romassist HTTP Server

```
*    /api/**  REST API
WS   /ws/**   WebSocket
GET  /**      Static files
```

## REST API Overview

```
GET /api/meta/flags => array of 32 strings (constant)
GET /api/meta/platform?detail => [name...] (detail="id", default) or [{v,c}...] (detail="record")
GET /api/meta/author?detail => [name...] (detail="id", default) or [{v,c}...] (detail="record")
GET /api/meta/genre?detail => [name...] (detail="id", default) or [{v,c}...] (detail="record")

GET /api/game/count => integer
GET /api/game?index&count => Game[]
GET /api/game?id&detail => Game
PUT /api/game?id&detail <= Game => Game
PATCH /api/game?id&detail <= Game => Game (must exist)
DELETE /api/game?id => nothing

GET /api/comment/count => integer
GET /api/comment?index&count => Comment[]
GET /api/comment?gameid&time&k => Comment[] (normal to omit time and k)
PUT /api/comment?gameid&k&v => Comment
PATCH /api/comment?gameid&time&k&v  => Comment (must exist)
DELETE /api/comment?gameid&time&k => nothing

GET /api/play/count => integer
GET /api/play?index&count => Play[]
GET /api/play?gameid&since => Play[]
PUT /api/play?gameid&dur => Play (always new)
PATCH /api/play?gameid&time&dur => Play (must exist. unusual. prefer POST /api/play/terminate)
POST /api/play/terminate?gameid => Play (must exist)
DELETE /api/play?gameid&time => nothing

GET /api/launcher/count => integer
GET /api/launcher?index&count => Launcher[]
GET /api/launcher?launcherid => Launcher
GET /api/launcher?gameid => Launcher (dynamically selects the appropriate one)
PUT /api/launcher <= Launcher => Launcher (always new)
PATCH /api/launcher <= Launcher => Launcher (must exist)
DELETE /api/launcher?launcherid => nothing

GET /api/list/count => integer
GET /api/list?index&count&detail => List[]
GET /api/list?listid&detail => List
GET /api/list?gameid&detail => List[] (those including gameid)
PUT /api/list?listid&detail <= List => List (always new)
PATCH /api/list?listid&detail <= List => List (must exist. if "games" present, they replace the whole array)
DELETE /api/list?listid => nothing
POST /api/list/add?listid&gameid&detail => List
POST /api/list/remove?listid&gameid&detail => List

GET /api/blob/all => [path...]
GET /api/blob?gameid&type => [path...]
GET /api/blob?path => raw content. (path) is the real local fs path; if you call from localhost, you could read it directly instead.
PUT /api/blob?gameid&type&sfx <= raw => path (new blob)
DELETE /api/blob?path => nothing

POST /api/query?text&list&platform&author&genre&flags&notflags&rating&pubtime&detail&limit&page&sort => Game[]

POST /api/launch?gameid => nothing
POST /api/terminate => nothing
```

## /api/meta

Constant-ish metadata from the database.

`GET /api/meta/flags` is literally constant; it returns a hard-coded list of strings for the flag names.

`platform`, `author`, and `genre` are generated dynamically on each request, based on values in the games table.
Use `detail="id"` or omit for a simple array of strings, or `detail="record"` to get `{v,c}` the name and count of each.

## /api/game

## /api/comment

## /api/play

## /api/launcher

## /api/list

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

After filtering, we can sort, paginate, and format to taste:

- `detail=record`: Same as Game calls.
- `limit=100`: Maximum results to return.
- `page=1`: One-based page index, if `limit` in play.
- `sort`: Order of results. TODO haven't decided what we're doing here.

The HTTP response will include a header `X-Page-Count` when paginated.

## /api/launch, /api/terminate

Launches a game.
Any game in progress will be terminated first.
A new `play` record will be added.

Or `/api/terminate` to stop whatever game is in progress and return to the menu.
The menu itself does not terminate on this, though it technically could.
