// This topic is operating the P+F PGV100

#include "ros/ros.h"
#include "std_msgs/String.h"
#include <pf_pgv100/pgv_scan_data.h>
// C library headers
#include <stdio.h>
#include <string.h>
#include <iostream>
// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWRs
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()
#include <bitset>
#include <string>
#include <stdlib.h>     /* strtoull */
#include <signal.h>
#include <sstream>

using namespace std;

// Set the port which your device is connected.

//using namespace std;
// To handle CTRL + C Interrupt
void my_handler(int s);
int serial_port = open("/dev/ttyUSB0", O_RDWR);

int main(int argc, char **argv)
{
  
   // To handle CTRL + C Interrupt
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
      

// Check for errors
if (serial_port < 0) {
    printf("Error %i from open: %s\n", errno, strerror(errno));
}

// Create new termios struc, we call it 'tty' for convention
struct termios tty;
memset(&tty, 0, sizeof tty);

// Read in existing settings, and handle any error
if(tcgetattr(serial_port, &tty) != 0) {
    printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
}

// Serial Communication Configuration
tty.c_cflag |= PARENB;  // Set parity bit, enabling parity
tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
tty.c_cflag |= CS7; // 8 bits per byte (most common)
tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
tty.c_lflag &= ~ICANON;
tty.c_lflag &= ~ECHO; // Disable echo
tty.c_lflag &= ~ECHOE; // Disable erasure
tty.c_lflag &= ~ECHONL; // Disable new-line echo
tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
// tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT IN LINUX)
// tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT IN LINUX)
tty.c_cc[VTIME] = 0;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
tty.c_cc[VMIN] = 2;
// Set in/out baud rate to be 115200
cfsetispeed(&tty, B115200);
cfsetospeed(&tty, B115200);

// Save tty settings, also checking for error
if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
    printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
}   

unsigned char dir_straight[ 2 ] = {0xEC, 0x13}; // Straight ahead 
unsigned char pos_req[ 2 ] = { 0xC8, 0x37}; // Position Request
string selected_dir = "Straight ahead";
write(serial_port,  dir_straight , sizeof( dir_straight));

ROS_INFO(" Direction set to <> Straight Ahead <>");

  /**
   * The ros::init() function needs to see argc and argv so that it can perform
   * any ROS arguments and name remapping that were provided at the command line.
   * For programmatic remappings you can use a different version of init() which takes
   * remappings directly, but for most command-line programs, passing argc and argv is
   * the easiest way to do it.  The third argument to init() is the name of the node.
   *
   * You must call one of the versions of ros::init() before using any other
   * part of the ROS system.
   */
  ros::init(argc, argv, "pgv100_node");

  /**
   * NodeHandle is the main access point to communications with the ROS system.
   * The first NodeHandle constructed will fully initialize this node, and the last
   * NodeHandle destructed will close down the node.
   */
  ros::NodeHandle n;

  /**
   * The advertise() function is how you tell ROS that you want to
   * publish on a given topic name. This invokes a call to the ROS
   * master node, which keeps a registry of who is publishing and who
   * is subscribing. After this advertise() call is made, the master
   * node will notify anyone who is trying to subscribe to this topic name,
   * and they will in turn negotiate a peer-to-peer connection with this
   * node.  advertise() returns a Publisher object which allows you to
   * publish messages on that topic through a call to publish().  Once
   * all copies of the returned Publisher object are destroyed, the topic
   * will be automatically unadvertised.
   *
   * The second parameter to advertise() is the size of the message queue
   * used for publishing messages.  If messages are published more quickly
   * than we can send them, the number here specifies how many messages to
   * buffer up before throwing some away.
   */
  ros::Publisher pgv100_pub = n.advertise<pf_pgv100::pgv_scan_data>("pgv100", 1000);

  ros::Rate loop_rate(10);

  /**
   * A count of how many messages we have sent. This is used to create
   * a unique string for each message.
   */

  int count = 0;
  while (ros::ok())
  {
    write(serial_port, pos_req , 2);
    char read_buf [256];
    memset(&read_buf, '\0', sizeof(read_buf));
    int byte_count = read(serial_port, &read_buf, sizeof(read_buf));
    
    // Get the angle from the byte array [Byte 11-12]
    std::bitset<7> ang_1(read_buf[10]);
    std::bitset<7> ang_0(read_buf[11]);
    std::string agv_ang_str = ang_1.to_string() + ang_0.to_string();
    int strlength = agv_ang_str.length();
    char agv_ang_char[strlength + 1];
    strcpy(agv_ang_char, agv_ang_str.c_str());
    char *angEnd;
    float agv_ang_des = strtoull(agv_ang_char, &angEnd, 2);

    // Get the X-Position from the byte array [Bytes 3-4-5-6]
    bitset<3> x_pos_3(read_buf[2]);
    bitset<7> x_pos_2(read_buf[3]);
    bitset<7> x_pos_1(read_buf[4]);
    bitset<7> x_pos_0(read_buf[5]);
    string agv_x_pos_str = x_pos_3.to_string() + x_pos_2.to_string() + x_pos_1.to_string() + x_pos_0.to_string();
    strlength = agv_x_pos_str.length();
    char agv_x_pos_char[strlength + 1];
    strcpy(agv_x_pos_char, agv_x_pos_str.c_str());
    char *xposEnd;
    unsigned long int agv_x_pos_des = strtoull(agv_x_pos_char, &xposEnd, 2);

    // Get Y-Position from the byte array [Bytes 7-8]
    bitset<7> y_pos_1(read_buf[6]);
    bitset<7> y_pos_0(read_buf[7]);
    string agv_y_pos_str = y_pos_1.to_string() + y_pos_0.to_string();
    strlength = agv_y_pos_str.length();
    char agv_y_pos_char[strlength + 1];
    strcpy(agv_y_pos_char, agv_y_pos_str.c_str());
    char *yposEnd;
    int agv_y_pos_des = strtoull(agv_y_pos_char, &yposEnd, 2);
    if (agv_y_pos_des > 2000)
    {
      agv_y_pos_des = agv_y_pos_des - 16384;
    }
    /**
     * This is a message object. You stuff it with data, and then publish it.
     */
    pf_pgv100::pgv_scan_data msg;
    // std_msgs::String msg;
    // std::stringstream ss;
    // ss << agv_ang_des/10; 
    // msg.data = ss.str();
    msg.angle = agv_ang_des/10; // degree
    msg.x_pos = agv_x_pos_des/10; // mm
    msg.y_pos = agv_y_pos_des; // mm
    msg.direction = selected_dir;
    //ROS_INFO("AGV Angle: %s", msg.data.c_str());

    /**
     * The publish() function is how you send messages. The parameter
     * is the message object. The type of this object must agree with the type
     * given as a template parameter to the advertise<>() call, as was done
     * in the constructor above.
     */
    pgv100_pub.publish(msg);

    ros::spinOnce();

    loop_rate.sleep();
    ++count;
    sigaction(SIGINT, &sigIntHandler, NULL);
  }


  return 0;
}

void my_handler(int s){
           close(serial_port);
           printf("\n\nCaught signal %d\n Port closed.\n",s);
           exit(1); 
}