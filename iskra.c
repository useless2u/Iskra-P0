// Iskra P0 ( infrared comm ) mqtt publisher for Domoticz
// use a cronjob on the pi 
// sudo nano /etc/crontab
// */5 * * * * root /home/pi/iskra-meter/iskra

// to compile:
// install paho-mqtt client
// git clone http://git.eclipse.org/gitroot/paho/org.eclipse.paho.mqtt.c.git
// cd org.eclipse.paho.mqtt.c
// make
// sudo make install

// gcc -o iskra iskra.c -lpaho-mqtt3c  -lpthread


#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include "MQTTClient.h"

#define ADDRESS     "tcp://192.168.178.21:1883"
#define CLIENTID    "ExampleClientPub"
#define TOPIC       "domoticz/in"
//#define PAYLOAD     "{\"idx\":24,\"nvalue\":0,\"svalue\":\"99\"}"
#define QOS         1
#define TIMEOUT     10000L

char PAYLOAD[120];


void error_message(char *message,int errorno){
printf(message,errorno);
}

int read_serial_byte(int fd){
unsigned char c = 0;
if (read(fd,&c,1) !=1) return(0);
return (int) c & 0xff;
}

int write_serial_byte(int fd,int byte){
unsigned char c = byte & 0xff;
if (write(fd, &c,1) !=1) return(0);
return(1);
}

int read_serial_line(int fd,char *resp){
char rchar[2];
int num_read=0;
int position=0;
resp[position]=0;
rchar[1]='\0';
while (1) {
        num_read = read(fd, &rchar, 1);
        if (num_read != 0) {
                strcat(resp,rchar);
                if (rchar[0]=='\n') break;
                }
        }

return(0);
}


int send_serial(int fd,char *message){
write (fd, message, sizeof(message)+2);
return 0;
}

int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                error_message ("error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7;     // 7-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 15;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                error_message ("error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}


int publish_data() {

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }
    pubmsg.payload = PAYLOAD;
    pubmsg.payloadlen = strlen(PAYLOAD);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
//    printf("Waiting for up to %d seconds for publication of %s\n"
//            "on topic %s for client with ClientID: %s\n",
//            (int)(TIMEOUT/1000), PAYLOAD, TOPIC, CLIENTID);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
//    printf("Message with delivery token %d delivered\n", token);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;

}

void
set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                error_message ("error %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                error_message ("error %d setting term attributes", errno);
}



char *portname = "/dev/ttyUSB1";
int fd;
static char response[100];

int main(int argc, char *argv[]){

char *init_iskra="/?!\r\n";

printf("start...\n");

fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);

if (fd < 0)
{
//      error_message ("\nerror opening port errorno %s",strerror(errno));
        /*error_message ("error %d opening %s: %s", errno, portname, strerror (errno));*/
      fprintf (stderr, "%s: Couldn't open port %s; %s\n",
               argv[0], portname, strerror (errno));
      exit (EXIT_FAILURE);
        return;
}

set_interface_attribs (fd, B300, 1);  // set speed to 115,200 bps, 8n1 (no parity)
set_blocking (fd, 0);                // set no blocking


if (send_serial(fd,"/?!\r\n")) goto error1;           //  get identification
read_serial_line(fd,response);
if (strcmp(response,init_iskra)) goto error1;
read_serial_line(fd,response);
printf("IDstring=%s",response);
// check Identification string
if (response[0] != '/') goto error2;

char Manufacturer[5];
memcpy(Manufacturer, &response[1],4);
Manufacturer[4]=0;
printf("manufacturer:%s\n",Manufacturer);

char Id[35];
if (response[5] == '\\') {
memcpy(Id, &response[7],strlen(response) - 7 - 2);
Id[strlen(response)-7-2]=0;
} else {
memcpy(Id, &response[5],strlen(response) - 5 - 2);
Id[strlen(response)-5-2]=0;
}
printf("product Id:%s\n",Id);

int speed=response[4] - 48; // 0 (dec) = ascii 48, 5 = ascii 53
int Baudrate = B300;
switch (speed) {
case 1:
        Baudrate=B600;
        break;
case 2:
        Baudrate=B1200;
        break;
case 3:
        Baudrate=B2400;
        break;
case 4:
        Baudrate=B4800;
        break;
case 5:
        Baudrate=B9600;
        break;
case 6:
        Baudrate=B19200;
        break;
default:
        Baudrate=B300;
}

printf("new baudrate will be  = %d\n",Baudrate);
printf("sending Ack\n");

char *ack_str="000\r\n";
if (!write_serial_byte(fd,0x06)) goto error3;
if (send_serial(fd,ack_str)) goto error3;           //  get identification

unsigned char rec=0;
char idstr1[6]="1.8.0";

float value180;
float value280;

char tempstr[20];
while(1){
read_serial_line(fd,response);
//printf("line %s",response);
memcpy(idstr1, &response[4],5);
idstr1[5] = '\0';
//printf("idstr 1:%s\n",idstr1);
memcpy(tempstr, &response[14],6);
tempstr[6]='\0';

if (strcmp(idstr1,"1.8.0")==0) {
value180=atof(tempstr);
printf("1.8.0 = %s %6.3f\n",tempstr,value180);
}

if (strcmp(idstr1,"2.8.0")==0) {

value280=atof(tempstr);
printf("1.8.1 = %s %6.3f\n",tempstr,value280);
}

usleep(10 * 1000);
if (strcmp(response,"!\r\n")==0) break;

//if (str=='!') break;
}

sprintf(PAYLOAD,"{\"idx\":84,\"nvalue\":0,\"svalue\":\"%f\"}",value180 * 1000);

if (publish_data()==-1) goto error6;

sprintf(PAYLOAD,"{\"idx\":85,\"nvalue\":0,\"svalue\":\"%f\"}",value280 * 1000);

if (publish_data()==-1) goto error6;

//if (read_serial_byte(fd) != 0x02) goto error5;

return 0;

error1:
      fprintf (stderr, "initialization failed ( no response or wrong respons )\n");
      exit (EXIT_FAILURE);
error2:
      fprintf (stderr, "manufacturer id does not start with a \\n");
      exit (EXIT_FAILURE);
error3:
        fprintf (stderr, "send ack_string error\n");
        exit (EXIT_FAILURE);
error5:
        fprintf(stderr, "Did not receive STX\n");
        exit (EXIT_FAILURE);
error4:
        fprintf(stderr,"baudrate change failed\n");
        exit (EXIT_FAILURE);

error6:
        fprintf(stderr,"Publish failed\n");
        exit (EXIT_FAILURE);

//return 0;
}