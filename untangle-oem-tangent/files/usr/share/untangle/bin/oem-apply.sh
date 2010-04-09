#! /bin/sh

# apply files
rsync -Ha /usr/share/untangle-oem-*/ /

# rename grub titles
sed -i 's|^\(title.*\)Debian GNU/Linux, kernel|\1Kernel|' /boot/grub/menu.lst
sed -i 's|^\(title.*\)-untangle|\1|' /boot/grub/menu.lst

# change default settings
psql -U postgres uvm -c "update settings.u_mail_settings set from_address = 'webhawk@webhawk.untangle.com' where from_address = 'untangle@untangle.example.com'"
psql -U postgres uvm -c "update settings.u_address_settings set hostname = 'webhawk.example.com' where hostname = 'untangle.example.com'"