#!/bin/bash
MD5=$(md5sum  .pioenvs/esp12e/firmware.bin |cut -d' ' -f 1)
echo "$MD5" | ssh root@xn--2xa.ink -T "cat > /data/files/deepsleep-firmware.bin.md5"

echo "$MD5 Updated firmware version file deepsleep-firmware.bin.md5"
rsync -aPh .pioenvs/esp12e/firmware.bin "root@xn--2xa.ink:/data/files/firmware/$MD5.bin"
echo "$MD5 Uploaded firmware binary firmware/$MD5.bin"
echo "$@" | \
    ssh root@xn--2xa.ink \
    -T "cat > /data/files/firmware/$MD5.precmd"
echo "$MD5 Set precmd file firmware/$MD5.precmd"