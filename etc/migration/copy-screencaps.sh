#!/bin/sh

# We'll take a leap of faith and assume that all games in the v2 database retained their ID, and we can just copy the screencap files verbatim.

# Safe to add -p if you want to allow it to already exist. I'm assuming we only operate on fresh dbs.
mkdir ../../data/blob || exit 1

for BUCKET in ../../../ra2/data/screencaps/* ; do
  BBASE=$(basename $BUCKET)
  #echo "BUCKET: $BUCKET [$BBASE]"
  mkdir ../../data/blob/$BBASE || exit 1
  for SRCPATH in $BUCKET/*.png ; do
    if ( echo "$SRCPATH" | grep -qE '\*' ) ; then
      # If the source bucket is empty, as some of them are, globbing returns the unmatched pattern.
      continue
    fi
    SRCBASE=$(basename $SRCPATH)
    echo $SRCBASE | tr '.-' ' ' | (
      read GAMEID TIME DUMMY
      #echo "BUCKET=$BUCKET GAMEID=$GAMEID TIME=$TIME"
      cp $SRCPATH ../../data/blob/$BBASE/$GAMEID-scap-$TIME.png || exit 1
    ) || exit 1
  done
done
