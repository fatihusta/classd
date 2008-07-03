#! /bin/bash

## Delete all of the expired keys
rm -f /usr/share/untangle/conf/licenses/*-trial.ulf

## Run all of the generators
for t in /usr/share/untangle/bin/rup-* ; do
  bash $t
done

## Run all of the libitems to decompress the keys, this will also reload the licenses.
for t in `ls /var/lib/dpkg/info/untangle-libitem-trial{14,30}-*.postinst 2> /dev/null` ; do
  bash $t
done

#rush -c '%w(untangle-user-directory-integration untangle-policy-manager untangle-user-directory-management untangle-remote-access-portal untangle-configuration-backup untangle-dual-virus-blocker untangle-dual-virus-blocker-kav untangle-pcremote untangle-branding-manager).each { |n| puts "#{n} is expired: #{RUSH.uvm.license_manager.getLicenseStatus(n).is_expired}" }'

exit 0