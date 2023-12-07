#!/bin/bash

#---------------------------------------------------------------------------
# First, clone or pull, then build, for each of our source projects.
# It's important that ra3 itself be first in this list.
# These must all be repos in my GitHub account.

PROJDIR=$(realpath $(dirname $0)/../../..)
echo "PROJDIR=$PROJDIR"
if [ -z "$PROJDIR" ] ; then
  echo "$0: Failed to determine projects directory, please fix this script."
  exit 1
fi

PROJS="ra3 akz26 akprosys akfceu akgambatte aksnes9x"
PROJS="$PROJS rabbit chetyorska ctm fullmoon4 ivand lilsitter plundersquad pokorc sitter2009 sitter8 ttaq"

NO_MAKE_PROJS="sitter8"

for PROJ in $PROJS ; do
  PROOT="$PROJDIR/$PROJ"
  if [ -d "$PROOT" ] ; then
    echo "Pulling $PROJ..."
    ( cd $PROOT && git pull ) || exit 1
  else
    echo "Cloning $PROJ..."
    ( cd .. && git clone "https://github.com/aksommerville/$PROJ" ) || exit 1
  fi
  if ! grep -q "$PROJ" <<<"$NO_MAKE_PROJS" ; then
    echo "Building $PROJ..."
    make -C"$PROOT" || exit 1
  fi
done

#-----------------------------------------------------------------------
# Next prompt the user for DB migration.

read -p "Source HOST:PORT for DB migration, or empty to skip: " REMOTEHOST
if [ -n "$REMOTEHOST" ] ; then
  echo "Starting DB migration..."
  out/romassist --dbroot=data --migrate="$REMOTEHOST" || exit 1
else
  echo "Skipping DB migration."
fi

#----------------------------------------------------------------------
# If the systemd service is not installed, prompt to install now.
# Don't do it without prompting! That would be presumptuous.

if [ -f /etc/systemd/system/romassist.service ] ; then
  echo "Systemd service already installed."
else
  read -p "Install systemd service? [y/N] " CONFIRM
  if [ "$CONFIRM" = "y" ] ; then
    sudo cp etc/romassist.service /etc/systemd/system/romassist.service || exit 1
    sudo systemctl enable romassist || exit 1
    echo "Systemd service installed. Will autolaunch at next boot. 'sudo systemctl start romassist' to launch now."
  fi
fi

echo "Migration successful. Next you'll want to verify launcher args and input mapping, I'll leave that to you. Cheers\!"
