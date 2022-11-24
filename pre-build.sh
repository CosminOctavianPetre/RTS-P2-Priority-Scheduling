#!/bin/sh

cd ./rootfs;
tar cf ../build/music_rtems/tarfile *;
cd ../build/music_rtems;
${HOME}/rtems-dev/compiler/i386/5/bin/i386-rtems5-ld -r --noinhibit-exec -o tarfile.o -b binary tarfile
