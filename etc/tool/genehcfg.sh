#!/bin/sh

# We produce the script "emuhost-config", to deliver compile options to consumers.
# Put (CFLAGS,LDFLAGS,LIBS) in the environment when calling.
# Also INC: Include path for emuhost (won't be in the normal CFLAGS for romassist's build).
# Also EHLIB: It goes in LIBS, but also we can report it as a dependency.

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 OUTPUT"
  exit 1
fi

DSTPATH="$1"
shift 1

cat - >"$DSTPATH" <<EOF
#!/bin/sh
# Do not edit. Generated on $(date) by $0

if [ "\$#" -lt 1 ] ; then
  echo "Usage: \$0 --cflags|--ldflags|--libs|--deps"
  exit 1
fi

for ARG in \$* ; do
  case "\$ARG" in
    --cflags) echo -n "$CFLAGS -I$INC " ;;
    --ldflags) echo -n "$LDFLAGS " ;;
    --libs) echo -n "$EHLIB $LIBS " ;;
    --deps) echo -n "$EHLIB " ;;
  esac
done
echo ""
EOF

chmod +x "$DSTPATH"
