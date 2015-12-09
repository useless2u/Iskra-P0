Iskra P0 ( infrared comm ) mqtt publisher for Domoticz

use a cronjob on the pi
sudo nano /etc/crontab
*/5 * * * * root /home/pi/iskra-meter/iskra

to compile:
install paho-mqtt client
git clone http://git.eclipse.org/gitroot/paho/org.eclipse.paho.mqtt.c.git
cd org.eclipse.paho.mqtt.c
make
sudo make install

Review/edit port and ip settings in code!

gcc -o iskra iskra.c -lpaho-mqtt3c  -lpthread
