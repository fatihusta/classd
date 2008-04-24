#! /bin/bash

## Delete all of the expired keys
rm -f /usr/share/untangle/conf/licenses/*-trial.ulf

## Run all of the generators
for t in /usr/share/untangle/bin/rup-* ; do
  bash $t
done

## Run all of the libitems to decompress the keys, this will also reload the licenses.
for t in /var/lib/dpkg/info/untangle-libitem-trial{14,30}-*.postinst ; do
  bash $t
done
