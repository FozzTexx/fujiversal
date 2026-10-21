#!/bin/sh
# Run the ROMs in openMSX: on their own mapper they must pass, on a wrong one
# the windows must fail. Expected: windows then mirrors, P/F/- or *.
#   OPENMSX=... MACHINE=... ./run-openmsx.sh [build-dir]

BUILD=${1:-build}
HERE=$(cd "$(dirname "$0")" && pwd)
: "${OPENMSX:=$HOME/fujinet/openmsx/derived/aarch64-darwin-opt-3rd/bin/openmsx}"
: "${MACHINE:=Sanyo_PHC-23J}"

failed=0

# run <rom> <romtype> <expected> [expanded]
# expanded: subslot 1 of a slot expander, behind an empty ROM.
run() {
	out=$(mktemp)
	if [ "$4" = expanded ]; then
		slots="-exta slotexpander -cart $BUILD/empty.rom -cart"
	else
		slots="-carta"
	fi
	MAPPERTEST_OUT=$out "$OPENMSX" -machine "$MACHINE" \
	    $slots "$BUILD/$1" -romtype "$2" \
	    -script "$HERE/mappertest.tcl" >/dev/null 2>&1
	got=$(head -n 1 "$out")
	case $got in
	$3) pass=1 ;;
	*) pass= ;;
	esac
	if [ -n "$pass" ] && [ ${#got} -eq 2 ]; then
		echo "ok    $1 as $2${4:+, $4} (expected $3)"
	else
		echo "FAIL  $1 as $2${4:+, $4}: expected $3, got ${got:-nothing}"
		sed -e '1d' -e '/^ *$/d' -e 's/^/      /' "$out"
		failed=1
	fi
	rm -f "$out"
}

run mappertest-plain.rom     Normal    P-
run mappertest-konami.rom    Konami    PP
run mappertest-konamiscc.rom KonamiSCC PP
run mappertest-ascii8.rom    ASCII8    P-
run mappertest-ascii16.rom   ASCII16   P-
run mappertest-konami.rom    Konami    PP expanded
run mappertest-konamiscc.rom KonamiSCC PP expanded

run mappertest-plain.rom     ASCII16   F-
run mappertest-konami.rom    KonamiSCC F\*
run mappertest-konamiscc.rom Konami    F\*
run mappertest-ascii8.rom    Konami    F-
run mappertest-ascii16.rom   ASCII8    F-

exit $failed
