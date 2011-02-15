#!/bin/dash

#LOGGER="/usr/bin/logger -t $0"
LOGGER="echo"
COOKIEJAR=`mktemp`
STORE_URL='https://staging-store.untangle.com'
CREDS=/etc/untangle/oem/oemid
UID_FILE=/usr/share/untangle/conf/uid

if [ ! -f $UID_FILE ]; then
  # no UID found, we better generate one.
  $LOGGER "Creating UID..."
  cp /usr/share/untangle/bin/ut-activate /tmp/
  sed -i 's/.*ut-register.*//' /tmp/ut-activate
  /tmp/ut-activate
  rm -f /tmp/ut-activate
fi

MYUID="`cat /usr/share/untangle/conf/uid | head -c 19`"

if [ "$MYUID" -eq "" ]; then
  $LOGGER "ERROR: UID not set properly."
fi

$LOGGER "Downloading Package cache..."
apt-get update
if [ ! $? -eq 0 ] ; then
    $LOGGER "ERROR: Downloading packages failed. Are you online?"
    exit 1
fi

if [ -f $CREDS ]; then
  EMAIL=`cat $CREDS | grep EM | cut -f2 -d" "`
  PASSWORD=`cat $CREDS | grep PW | cut -f2 -d" "`
else
  if [ -f $COOKIEJAR ] ; then
    rm $COOKIEJAR
  fi
  $LOGGER "ERROR: No login information found."
  exit 1
fi

$LOGGER "Logging in with $EMAIL ..."

LOGIN_URL="https://staging-store.untangle.com/untangle_admin/oem/oemapi.php?action=login&email=$EMAIL&password=$PASSWORD&uid=$MYUID"
REG_URL="https://staging-store.untangle.com/untangle_admin/oem/oemapi.php?action=register"

LOGIN="`curl -ksS -c $COOKIEJAR $LOGIN_URL`"
REGISTER=`curl -ksS -b$COOKIEJAR $REG_URL`
if [ ! $? -eq 0 ]; then
  $LOGGER "ERROR: Couldn't reach the store."
  exit 1
fi
if [ "$REGISTER" = "ERROR" ]; then
  $LOGGER "ERROR: Couldn't process registration."
  exit 1
fi

LIBITEMS=`echo $REGISTER | cut -f1 -d"%"`
NODES=`echo $REGISTER | cut -f2 -d"%"`

$LOGGER "apt-get install --yes --force-yes $LIBITERMS"
apt-get install --yes --force-yes $LIBITEMS

if [ ! $? -eq 0 ] ; then
    $LOGGER "ERROR: Failed to download libitems successfully"
    exit 1
fi

$LOGGER "Restarting untangle-vm"
/etc/init.d/untangle-vm restart

if [ -f $COOKIEJAR ] ; then
  rm $COOKIEJAR
fi

# regenerate ssh keys
$LOGGER "Regenrating SSH Keys"
ssh-keygen -q -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa
ssh-keygen -q -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa

/etc/init.d/ssh restart

#
# unset password
#
$LOGGER "Unsetting password..."
if ! sudo grep -qE '^root:(\*|YKN4WuGxhHpIw|$1$3kRMklXp$W/hDwKvL8GFi5Vdo3jtKC\.|CHANGEME):' /etc/shadow ; then
    echo "Resetting root password"
    perl -pe "s/^root:.*?:/root:CHANGEME:/" /etc/shadow > /tmp/newshadow
    cp -f /tmp/newshadow /etc/shadow
    rm -f /tmp/newshadow
fi

exit 0
