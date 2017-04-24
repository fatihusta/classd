#! /bin/bash

# apply files
rsync -Ha /usr/share/untangle-oem-*/ /

# rename grub titles
if [ -f /boot/grub/menu.lst ] ; then
    sed -i 's|^\(title.*\)Debian GNU/Linux, kernel|\1Kernel|' /boot/grub/menu.lst
    sed -i 's|^\(title.*\)-untangle|\1|' /boot/grub/menu.lst
fi

# change startup messages
sed -i 's|OEM_NAME=.*|OEM_NAME=\"APC\"|' /etc/init.d/untangle-vm

# set default hostname if still default
if [ "`cat /etc/hostname`" == "untangle.example.com" ] ; then
    echo "apc.example.com" > /etc/hostname
    hostname "apc.example.com"
fi
if [ ! -f /etc/hostname ] ; then
    echo "apc.example.com" > /etc/hostname
    hostname "apc.example.com"
fi

exit 0
