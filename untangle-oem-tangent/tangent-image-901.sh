#!/bin/bash

if [ ! $# -eq 1 ] ; then
    echo "usage $0 <VOUCHER>"
    exit 1
fi

#
# kill firefox
#
echo "Closing firefox..."
killall firefox-bin &> /dev/null
killall /usr/lib/iceweasel/firefox-bin &> /dev/null

#
# create UID
#
echo "Creating UID..."
/usr/share/untangle-/bin/ut-createUID
#cp /usr/share/untangle/bin/ut-activate /tmp/
#sed -i 's/.*ut-register.*//' /tmp/ut-activate
#/tmp/ut-activate
#rm -f /tmp/ut-activate

echo "Downloading Package cache..."
sleep 5
apt-get update
if [ ! $? -eq 0 ] ; then
    echo "ERROR: Downloading packages failed. Are you online?"
    exit 1
fi

#
# register UID with store
#
MYUID="`cat /usr/share/untangle/conf/uid | head -c 19`"
#VOUCHER="ALD1210-20100305A2GSF3OTP7F"
VOUCHER=$1
CUSTOMERID="5873"
URL="http://store.untangle.com/untangle_admin/oem/redeem-voucher.php?vc=$VOUCHER&uid=$MYUID&sid=$CUSTOMERID"

echo "Redeeming Voucher..."
sleep 5
OUTPUT="`curl $URL`"

if [ ! $? -eq 0 ] ; then
    echo "ERROR: $OUTPUT"
    echo "ERROR: is the voucher valid? Call Untangle"
    exit 1
fi
if [ ! "$OUTPUT" = "success" ] ; then
    echo "ERROR: $OUTPUT"
    echo "ERROR: is the voucher valid? Call Untangle"
    exit 1
fi

echo "Successfully redeemed Voucher"
sleep 5

#
# download apps
#
# Tangent libitem list:
#untangle-libitem-protofilter
#untangle-libitem-reporting
#untangle-libitem-shield
#untangle-libitem-sitefilter
#untangle-libitem-bandwidth
#untangle-libitem-cpd

OTHER="untangle-support-agent"
LIBITEMS="untangle-libitem-cpd untangle-libitem-protofilter untangle-libitem-reporting untangle-libitem-shield untangle-libitem-sitefilter untangle-libitem-bandwidth"
RACK_NODES="untangle-node-protofilter untangle-node-sitefilter untangle-node-bandwidth"
SERVICE_NODES="untangle-node-reporting untangle-node-reporting untangle-node-cpd untangle-node-shield"

echo "apt-get install --yes --force-yes $OTHER"
apt-get install --yes --force-yes $OTHER

echo "apt-get install --yes --force-yes $LIBITEMS"
apt-get install --yes --force-yes $LIBITEMS

if [ ! $? -eq 0 ] ; then
    echo "ERROR: Failed to download libitems successfully - call Untangle or email support@untangle.com"
    exit 1
fi

#
# install oem package
#
echo "apt-get install --yes --force-yes untangle-oem-tangent"
apt-get install --yes --force-yes untangle-oem-tangent

#
# Hide the the left bar (this overwrites the existing oem properties)
# this should be removed when the new untangle-oem-tangent package is available
# in the public repository
#
rm /etc/untangle/oem/oem.properties
cat >/etc/untangle/oem/oem.properties <<ENDOFTEXT
uvm.oem.name = WebHawk
uvm.oem.url = http://w3hawk.com
uvm.legal.url = http://legal.untangle.com
uvm.store.url = https://store.w3hawk.com
uvm.help.url = http://www.w3hawk.com/get.php
uvm.hidden.libitems = untangle-libitem-router,untangle-libitem-splitd,untangle-libitem-trial14-splitd,untangle-libitem-commtouch,untangle-libitem-trial14-commtouch,untangle-libitem-webfilter,untangle-libitem-lite-package,untangle-libitem-standard-package,untangle-libitem-trial14-standard-package,untangle-libitem-premium-package,untangle-libitem-trial14-premium-package,untangle-libitem-commtouch,untangle-libitem-trial14-commtouch,untangle-libitem-adblocker,untangle-libitem-clam,untangle-libitem-firewall,untangle-libitem-ips,untangle-libitem-opensource-package,untangle-libitem-openvpn,untangle-libitem-phish,untangle-libitem-spamassassin,untangle-libitem-spyware,untangle-libitem-adconnector,untangle-libitem-trial14-adconnector,,untangle-libitem-boxbackup,untangle-libitem-branding,untangle-libitem-faild,untangle-libitem-trial14-faild,untangle-libitem-kav,untangle-libitem-trial14-kav,untangle-libitem-policy,untangle-libitem-trial14-policy,untangle-libitem-splitd,untangle-libitem-trial14-splitd,untangle-libitem-support,untangle-libitem-webcache,untangle-libitem-trial14-webcache
ENDOFTEXT


# restart so untangle-vm sees new nodes (ucli register doesn't work pre-registration)
/etc/init.d/untangle-vm restart

#
# install apps in rack
#
for i in $RACK_NODES ; do 
    echo "Installing $i" 
    TID="`ucli -p 'Default Rack' instantiate $i`"
    if [ ! $? -eq 0 ] ; then
        echo "ERROR: Failed to install libitems successfully - call Untangle or email support@untangle.com"
        exit 1
    fi    
    ucli start $TID &> /dev/null
    # ignore return code (some aren't supposed to start)
done

for i in $SERVICE_NODES ; do 
    echo "Installing $i" 
    TID="`ucli instantiate $i`"
    if [ ! $? -eq 0 ] ; then
        echo "ERROR: Failed to install libitems successfully - call Untangle or email support@untangle.com"
        exit 1
    fi    
    ucli start $TID &> /dev/null
    # ignore return code (some aren't supposed to start)
done

#
# unset password
#
echo "Unsetting password..."
if ! sudo grep -qE '^root:(\*|YKN4WuGxhHpIw|$1$3kRMklXp$W/hDwKvL8GFi5Vdo3jtKC\.|CHANGEME):' /etc/shadow ; then
    echo "Resetting root password"
    perl -pe "s/^root:.*?:/root:CHANGEME:/" /etc/shadow > /tmp/newshadow
    cp -f /tmp/newshadow /etc/shadow
    rm -f /tmp/newshadow
fi

#
# power off
#
echo "Finished"
