# Sourced by the other scripts: which game they work on, from $GAME.
#   GAME=m2 (default)   Marathon 2: Durandal          -> Durandal.app
#   GAME=inf            Marathon Infinity             -> Durandal Infinity.app
#   GAME=m1             Marathon                      -> Durandal Marathon.app
# Sets GAME_TITLE, GAME_SCHEME (Xcode), GAME_APP_NAME (bundle and binary
# name), GAME_DATA (scenario folder), GAME_REPLAYS (upstream's test films),
# GAME_DEMO (a default film), GAME_DATA_FILES (what a bundle must carry),
# and game_app <config> -> the built binary.
# Needs $ROOT set to the repository.
case ${GAME:-m2} in
  m2|marathon2|durandal)
    GAME=m2; GAME_TITLE="Marathon 2"; GAME_SCHEME="Marathon 2"
    GAME_APP_NAME="Durandal"; GAME_SCENARIO="Marathon 2"
    GAME_DATA_FILES=(Map.sceA Shapes.shpA Sounds.sndA Images.imgA "Physics Models")
    GAME_DEMO_NAME="Demos/L00.filA" ;;
  inf|infinity|m3)
    GAME=inf; GAME_TITLE="Marathon Infinity"; GAME_SCHEME="Marathon 3"
    GAME_APP_NAME="Durandal Infinity"; GAME_SCENARIO="Marathon Infinity"
    GAME_DATA_FILES=(Map.sceA Shapes.shpA Sounds.sndA Images.imgA "Physics Models")
    GAME_DEMO_NAME="Demos/LA COSA NOSTRA.filA" ;;
  m1|marathon)
    GAME=m1; GAME_TITLE="Marathon"; GAME_SCHEME="Marathon 1"
    GAME_APP_NAME="Durandal Marathon"; GAME_SCENARIO="Marathon"
    GAME_DATA_FILES=(Map.scen Shapes.shps Sounds.sndz Physics.phys)
    GAME_DEMO_NAME="" ;;
  *) echo "GAME must be m2, inf or m1 (was: $GAME)" >&2; exit 2 ;;
esac
GAME_DATA="$ROOT/data/Scenarios/$GAME_SCENARIO"
GAME_REPLAYS="$ROOT/tests/replays/$GAME_SCENARIO"
GAME_DEMO=${GAME_DEMO_NAME:+$GAME_DATA/$GAME_DEMO_NAME}
# Marathon 1 ships no demo films: the first upstream test film stands in.
[[ -z $GAME_DEMO ]] && GAME_DEMO="$GAME_REPLAYS/Tooncinator Films/M1 L1 Arrival.42903.filA"
game_app() {
  echo "$ROOT/.deps/DerivedData/Build/Products/${1:-Release}/$GAME_APP_NAME.app/Contents/MacOS/$GAME_APP_NAME"
}
