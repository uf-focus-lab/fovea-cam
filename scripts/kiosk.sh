#!/bin/bash
BASE=/home/data/
LOG_DIR=/$BASE/FoveaCam.log

while true; do
    # Ensure the log directory exists
    mkdir -p $LOG_DIR
    # Create a new directory for each run
    TS=$(date +%y%m%d-%H%M%S)
    DIR=$BASE/$TS
    mkdir -p $DIR && cd $DIR
    # Run the FoveaCam binary
    CMD=$(FoveaCam)
    sudo $CMD 1> $LOG_DIR/$TS.txt 2> $LOG_DIR/$TS.log
    # Remove the directory if it's empty
    if [ -z "$(ls -A $DIR)" ]; then
        rm -rf $DIR
    fi
done
