#!/bin/sh

GAMELIST=game.json
# Was going to add lists too, but my current ra2 db actually doesn't have any. Lucky.

curl -s 'http://localhost:6502/api/db/query?detail=comments' | sed 's/}\,{/}\n\,{/g' > $GAMELIST
