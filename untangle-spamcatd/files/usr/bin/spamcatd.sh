#!/bin/dash

export LD_LIBRARY_PATH=/usr/lib/spamcatd:$LD_LIBRARY_PATH

exec /usr/bin/spamcatd /etc/spamcatd/spamcatd.conf >> /var/log/untangle-spamcatd.log
