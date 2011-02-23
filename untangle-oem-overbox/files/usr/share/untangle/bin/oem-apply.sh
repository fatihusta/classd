#! /bin/sh

# apply files
rsync -Ha /usr/share/untangle-oem-*/ /

# rename grub titles
sed -i 's|^\(title.*\)Debian GNU/Linux, kernel|\1Kernel|' /boot/grub/menu.lst
sed -i 's|^\(title.*\)-untangle|\1|' /boot/grub/menu.lst

# change default settings
psql -U postgres uvm -c "update settings.u_mail_settings set from_address = 'info@overbox.es'"
psql -U postgres uvm -c "update settings.u_address_settings set hostname = 'overbox.es'"

# set default hostname if still default
if [ "`cat /etc/hostname`" == "untangle.example.com" ] ; then
    echo "overbox.es" > /etc/hostname
    hostname "overbox.es"
fi
if [ ! -f /etc/hostname ] ; then
    echo "overbox.es" > /etc/hostname
    hostname "overbox.es"
fi

# change alpaca help
sed -i 's/\(HELP.*\)www.untangle.com/\1overbox.es/' /var/lib/rails/untangle-net-alpaca/public/javascripts/e/glue.js

# set bootsplash
splashy_config -s overbox
update-initramfs -u

# replace license files
LIC_FILE=/usr/share/untangle/toolbox/untangle-node-license-impl/LicenseProfessional.txt
if [ -f $LIC_FILE ] ; then
    cat > $LIC_FILE <<EOF
Contact MSPX for more details.
EOF
fi
LIC_FILE=/usr/share/untangle/lib/untangle-libuvm-api/LicenseStandard.txt
if [ -f $LIC_FILE ] ; then
    cat > $LIC_FILE <<EOF
Contact MSPX for more details.
EOF
fi

# flush firefox cache
/bin/rm -rf /home/kiosk/.mozilla/firefox/`grep Path /home/kiosk/.mozilla/firefox/profiles.ini | cut -f 2 -d=`/Cache/*

exit 0
