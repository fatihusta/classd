#!/bin/bash

#
# create UID
#
echo "Creating UID..."
cp /usr/share/untangle/bin/utactivate /tmp/
sed -i 's/.*utregister.*//' /tmp/utactivate
/tmp/utactivate
rm -f /tmp/utactivate

echo "Downloading Package cache..."
sleep 5
apt-get update
if [ ! $? -eq 0 ] ; then
    echo "Downloading packages failed. Are you online?"
    exit 1
fi

#
# register UID with store
#
echo "XXX IMPLEMENT ME"
echo "XXX IMPLEMENT ME"
# in the meantime just hack it to download locally
sed -i 's/updates.untangle.com/mephisto./' /etc/apt/sources.list.d/untangle.list
apt-get update
# wait 1 minute for ACL to take effect
echo "XXX IMPLEMENT ME"
echo "XXX IMPLEMENT ME"


#
# download apps
#
# Tangent libitem list:
#untangle-libitem-adblocker
#untangle-libitem-clam
#untangle-libitem-cpd
#untangle-libitem-firewall
#untangle-libitem-ips
#untangle-libitem-opensource-package
#untangle-libitem-openvpn
#untangle-libitem-phish
#untangle-libitem-protofilter
#untangle-libitem-reporting
#untangle-libitem-shield
#untangle-libitem-spamassassin
#untangle-libitem-spyware
#untangle-libitem-adconnector
#untangle-libitem-boxbackup
#untangle-libitem-branding
#untangle-libitem-faild
#untangle-libitem-kav
#untangle-libitem-policy
#untangle-libitem-professional-package
#untangle-libitem-sitefilter
#untangle-libitem-splitd
#untangle-libitem-support

LIBITEMS="untangle-libitem-adblocker untangle-libitem-clam untangle-libitem-cpd untangle-libitem-firewall untangle-libitem-ips untangle-libitem-opensource-package untangle-libitem-openvpn untangle-libitem-phish untangle-libitem-protofilter untangle-libitem-reporting untangle-libitem-shield untangle-libitem-spamassassin untangle-libitem-spyware untangle-libitem-webfilter untangle-libitem-adconnector untangle-libitem-boxbackup untangle-libitem-branding untangle-libitem-faild untangle-libitem-kav untangle-libitem-policy untangle-libitem-professional-package untangle-libitem-sitefilter untangle-libitem-splitd untangle-libitem-support"
RACK_NODES="untangle-node-clam untangle-node-firewall untangle-node-ips untangle-node-phish untangle-node-protofilter untangle-node-spamassassin untangle-node-spyware untangle-node-kav untangle-node-sitefilter"
SERVICE_NODES="untangle-node-openvpn untangle-node-reporting untangle-node-adconnector untangle-node-boxbackup untangle-node-faild untangle-node-policy untangle-node-splitd untangle-node-support untangle-node-shield"

echo "apt-get install --yes --force-yes $LIBITEMS"
apt-get install --yes --force-yes $LIBITEMS

if [ ! $? -eq 0 ] ; then
    echo "Failed to download libitems successfully - call Untangle or email dmorris@untangle.com"
    exit 1
fi

# restart so untangle-vm sees new nodes (ucli register doesn't work pre-registration)
/etc/init.d/untangle-vm restart

#
# install apps in rack
#
for i in $RACK_NODES ; do 
    echo "Installing $i" 
    TID="`ucli -p 'Default Rack' instantiate $i`"
    if [ ! $? -eq 0 ] ; then
        echo "Failed to install libitems successfully - call Untangle or email dmorris@untangle.com"
        exit 1
    fi    
    ucli start $TID &> /dev/null
    # ignore return code (some aren't supposed to start)
done

for i in $SERVICE_NODES ; do 
    echo "Installing $i" 
    TID="`ucli instantiate $i`"
    if [ ! $? -eq 0 ] ; then
        echo "Failed to install libitems successfully - call Untangle or email dmorris@untangle.com"
        exit 1
    fi    
    ucli start $TID &> /dev/null
    # ignore return code (some aren't supposed to start)
done

#
# turn off auto-upgrade
#
echo "Turning off auto-upgrade"
psql -U postgres uvm -c "update settings.u_upgrade_settings set auto_upgrade = 'f' where auto_upgrade = 't'"

#
# unset password
#
if ! sudo grep -qE '^root:(\*|YKN4WuGxhHpIw|$1$3kRMklXp$W/hDwKvL8GFi5Vdo3jtKC\.|CHANGEME):' /etc/shadow ; then
    echo "Resetting root password"
    sed -i "s/^root:.*:14708:/root:CHANGEME:14708:/" /etc/shadow
fi

#
# power off
#
echo "Finished"