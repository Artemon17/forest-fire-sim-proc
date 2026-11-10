#!/bin/bash
set -u
BIN=./build

$BIN/fire-main "$@" &
MAIN=$!

sleep 0.3

$BIN/fire-sensors 2>/dev/null &
SENSORS=$!
$BIN/fire-teams 2>/dev/null &
TEAMS=$!
$BIN/fire-spread 2>/dev/null &
SPREAD=$!

wait $MAIN
RC=$?

kill -TERM $SENSORS $TEAMS $SPREAD 2>/dev/null
wait $SENSORS $TEAMS $SPREAD 2>/dev/null

rm -f /dev/shm/fire_state_standalone /dev/shm/sem.fire_*
exit $RC