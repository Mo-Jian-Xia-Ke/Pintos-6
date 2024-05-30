#!/usr/bin/env bash
make
cd build || exit

# We need to shutdown QEMU opened last time as CLion is not able to shut it down upon stopping debugging.
killall qemu-system-i386

CMD="args-none" # Or change it to any test you want to debug

nohup bash -c "DISPLAY=window ../../utils/pintos --qemu --gdb  --filesys-size=2 -p tests/userprog/$CMD -a $CMD --swap-size=8 -- -q  -f run $CMD > pintos.log" & echo "Done!"

# Note that for vm-specific tests, change tests/userprog correspondingly
