#! /bin/sh

# apply files
rsync -Ha /usr/share/untangle-oem-*/ /

# rename grub titles
sed -i 's|^\(title.*\)Debian GNU/Linux, kernel|\1Kernel|' /boot/grub/menu.lst
sed -i 's|^\(title.*\)-untangle|\1|' /boot/grub/menu.lst

# change default settings
psql -U postgres uvm -c "update settings.u_mail_settings set from_address = 'admin@admin.example.com'"
psql -U postgres uvm -c "update settings.u_address_settings set hostname = 'admin.example.com'"

# set default hostname if still default
if [ "`cat /etc/hostname`" == "untangle.example.com" ] ; then
    echo "admin.example.com" > /etc/hostname
    hostname "admin.example.com"
fi
if [ ! -f /etc/hostname ] ; then
    echo "admin.example.com" > /etc/hostname
    hostname "admin.example.com"
fi

# change alpaca help
sed -i 's/\(HELP.*\)www.untangle.com/\1help.imaxnetbox.com/' /var/lib/rails/untangle-net-alpaca/public/javascripts/e/glue.js

# set bootsplash
splashy_config -s netbox
update-initramfs -u

# replace license files
LIC_FILE=/usr/share/untangle/toolbox/untangle-node-license-impl/LicenseProfessional.txt
if [ -f $LIC_FILE ] ; then
    cat > $LIC_FILE <<EOF
Contact iMax Netbox for more details.
EOF
fi
LIC_FILE=/usr/share/untangle/lib/untangle-libuvm-api/LicenseStandard.txt
if [ -f $LIC_FILE ] ; then
    cat > $LIC_FILE <<EOF
Contact iMax Netbox for more details.
EOF
fi

# flush firefox cache
/bin/rm -rf /home/kiosk/.mozilla/firefox/`grep Path /home/kiosk/.mozilla/firefox/profiles.ini | cut -f 2 -d=`/Cache/*

exit 0
