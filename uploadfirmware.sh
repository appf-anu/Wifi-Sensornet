#!/bin/bash
md5sum  .pioenvs/nodemcuv2/firmware.bin |cut -d' ' -f 1 > .pioenvs/nodemcuv2/firmware.bin.md5
cat .pioenvs/nodemcuv2/firmware.bin.md5
rsync -aPh .pioenvs/nodemcuv2/firmware.bin.md5 root@xn--2xa.ink:/data/files/
rsync -aPh .pioenvs/nodemcuv2/firmware.bin root@xn--2xa.ink:/data/files/firmware/$(md5sum  .pioenvs/nodemcuv2/firmware.bin | cut -d' ' -f1).bin