//================================================================================
// NOTE: This code is adapted from PacketClient.cpp, which remains in this
// folder. The license for the code is located in that file and duplicated
// below:
//
//=============================================================================
// Copyright © 2014 NaturalPoint, Inc. All Rights Reserved.
//
// This software is provided by the copyright holders and contributors "as is"
// and any express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular purpose
// are disclaimed. In no event shall NaturalPoint, Inc. or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or business
// interruption) however caused and on any theory of liability, whether in
// contract, strict liability, or tort (including negligence or otherwise)
// arising in any way out of the use of this software, even if advised of the
// possibility of such damage.
//=============================================================================

// #include <float.h>
#include <cmath>
// #include <cstdlib>
#include <cstring>
#include <optitrack.hpp>
// #include <iostream>     // std::cout
// #include <algorithm>    // std::min
#include <getopt.h>
#include <string.h>

#include <vector>
#include <stdio.h>
#include <unistd.h>  // read / write / sleep

#include <cstdlib>
#include <cstring>

#include <sys/time.h>

#include <QErrorMessage>
#include <QByteArray>
#include <QNetworkDatagram>
#include <QNetworkInterface>
// #include <QMutex>

// Linux socket headers
// #include <arpa/inet.h>
// #include <ifaddrs.h>
// #include <netdb.h>
// #include <netinet/in.h>
// #include <sys/ioctl.h>
// #include <sys/socket.h>
// #include <sys/types.h>
// #include <unistd.h>

#define MAX_NAMELENGTH 256

// NATNET message ids
#define NAT_PING 0
#define NAT_PINGRESPONSE 1
#define NAT_REQUEST 2
#define NAT_RESPONSE 3
#define NAT_REQUEST_MODELDEF 4
#define NAT_MODELDEF 5
#define NAT_REQUEST_FRAMEOFDATA 6
#define NAT_FRAMEOFDATA 7
#define NAT_MESSAGESTRING 8
#define NAT_UNRECOGNIZED_REQUEST 100
#define UNDEFINED 999999.9999

#define MAX_PACKETSIZE \
  100000  // max size of packet (actual packet size is dynamic)
#define VERBOSE 0

mocap_optitrack::mocap_optitrack(QWidget *parent) : QWidget(parent)
{

}

mocap_optitrack::~mocap_optitrack()
{
    if (Port != NULL)
    {
        if (iface != NULL)
        {
            Port->leaveMulticastGroup(QHostAddress(MULTICAST_ADDRESS), *iface);
            delete iface;
            iface = nullptr;
        }
        delete Port;
        Port = nullptr;
    }
}

bool mocap_optitrack::guess_optitrack_network_interface(QNetworkInterface &interface, QString &interface_address)
{
    // std::string interfaceAddress;
    //get a list of all network interfaces and ip addresses
    foreach (const QNetworkInterface &interface_, QNetworkInterface::allInterfaces())
    {
        QString interface_name = interface_.name();
        foreach(const QHostAddress &address, interface_.allAddresses())
        {

            if (address.protocol() == QAbstractSocket::IPv4Protocol)
            {
                // If the name starts with a 'w', then it is probably a wireless device
                if (interface_name[0] == 'w')
                {
                    interface_address = QString(interface_name);
                    interface = QNetworkInterface(interface_);
                    qDebug() << "[guess_optitrack_network_interface] detected interface address as " << interface_name;
                    return true;
                }
                // Else if it isn't the loopback device, then it's our best guess
                else if(interface_name.size() > 1)
                {
                    QString tmp = QString(interface_name);
                    tmp.chop(interface_name.size()-2);
                    if (QString::compare(interface_name, QString("lo"), Qt::CaseInsensitive) == 0)
                    {
                        interface_address = QString(interface_name);
                        interface = QNetworkInterface(interface_);
                        qDebug() << "[guess_optitrack_network_interface] detected interface address as " << interface_name;
                        return true;
                    }
                }

            }
        }
    }
    qDebug() << "[guess_optitrack_network_interface] failed to detect interface address";
    return false;
}

// SOCKET create_optitrack_data_socket_ipv6(const std::string& interfaceIp,
//                                          unsigned short port) {
//   SOCKET dataSocket = socket(AF_INET6, SOCK_DGRAM, 0);

//   // Prince: This variable is currently unused and causing a compiler warning
//   // However, I don't want to change the function prototype
//   port = port;

//   // allow multiple clients on same machine to use address/port
//   int value = 1;
//   int retval = setsockopt(dataSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&value,
//                           sizeof(value));
//   if (retval < 0) {
//     close(dataSocket);
//     return -1;
//   }

//   sockaddr_in6 socketAddr;
//   memset(&socketAddr, 0, sizeof(socketAddr));

//   socketAddr.sin6_family = AF_INET6;
//   socketAddr.sin6_port = htons(PORT_DATA);
//   socketAddr.sin6_addr = in6addr_any;

//   if (bind(dataSocket, (sockaddr*)&socketAddr, sizeof(sockaddr)) < 0) {
//     printf("[create_optitrack_data_socket] bind failed.\n");
//     return -1;
//   } else {
//     printf(
//         "[create_optitrack_data_socket] bound to socket successfully on port "
//         "%d\n",
//         PORT_DATA);
//   }

//   /*
//       // join multicast group
//       in_addr interfaceAddress;
//       interfaceAddress.s_addr = inet_addr(interfaceIp.c_str());

//       in_addr multicastAddress;
//       multicastAddress.s_addr = inet_addr(MULTICAST_ADDRESS);

//       ip_mreq Mreq;
//       Mreq.imr_multiaddr = multicastAddress;
//       Mreq.imr_interface = interfaceAddress;
//   */

//   // join multicast group
//   in6_addr interfaceAddress;
//   inet_pton(AF_INET6, interfaceIp.c_str(), &interfaceAddress);

//   in6_addr multicastAddress;
//   inet_pton(AF_INET6, MULTICAST_ADDRESS_6, &multicastAddress);

//   struct ipv6_mreq mreq6;
//   memcpy(&mreq6.ipv6mr_multiaddr, &multicastAddress, sizeof(struct in6_addr));

//   memcpy(&mreq6.ipv6mr_multiaddr, &interfaceAddress, sizeof(struct in6_addr));

//   // mreq6.ipv6mr_interface= //interfaceAddress;
//   /*
//           r1= setsockopt(dataSocket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
//                         &loopBack, sizeof(loopBack));
//           if (r1<0)
//                   perror("joinGroup:: IPV6_MULTICAST_LOOP:: ");
//   */

//   /*
//           int r2= setsockopt(dataSocket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
//                         &mcastTTL, sizeof(mcastTTL));
//           if (r2<0)
//                   perror("joinGroup:: IPV6_MULTICAST_HOPS::  ");
//   */

//   int r3 = setsockopt(dataSocket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6,
//                       sizeof(mreq6));
//   if (r3 < 0) perror("joinGroup:: IPV6_ADD_MEMBERSHIP:: ");

//   /*
//       retval = setsockopt(dataSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
//      (char*)&Mreq, sizeof(Mreq)); if (retval < 0) {
//           printf("[create_optitrack_data_socket] join failed.\n");
//           return -1;
//       } else {
//           printf("[create_optitrack_data_socket] joined multicast group at
//      address %s\n", MULTICAST_ADDRESS);
//       }
//   */

//   // create a 1MB buffer
//   int optval = 0x100000;
//   socklen_t optvalSize = 4;
//   setsockopt(dataSocket, SOL_SOCKET, SO_RCVBUF, (char*)&optval, optvalSize);
//   getsockopt(dataSocket, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optvalSize);
//   if (optval != 0x100000) {
//     printf("[create_optitrack_data_socket] ReceiveBuffer size = %d\n", optval);
//   } else {
//     printf(
//         "[create_optitrack_data_socket] Increased receive buffer size to %d\n",
//         optval);
//   }

//   return dataSocket;
// }

bool mocap_optitrack::create_optitrack_data_socket(\
            QNetworkInterface &interface, QString &interface_address, unsigned short port) {

    if (Port != NULL)
    {
        if (iface != NULL)
        {
            Port->leaveMulticastGroup(QHostAddress(MULTICAST_ADDRESS), *iface);
            delete iface;
            iface = nullptr;
        }
        delete Port;
        Port = nullptr;
    }
    Port = new QUdpSocket();

    if (!Port->bind(port, QUdpSocket::ShareAddress))
    {
        (new QErrorMessage)->showMessage(Port->errorString());
        return false;
    }


    if (Port->joinMulticastGroup(QHostAddress(MULTICAST_ADDRESS), interface))
    {
        qDebug() << "[create_optitrack_data_socket] joined multicast group at address " << QString(MULTICAST_ADDRESS);
    }
    else
    {
        (new QErrorMessage)->showMessage("[create_optitrack_data_socket] join failed.\n");
        return false;
    }
    // create a 1MB buffer
    int optval = 0x100000;

    Port->setReadBufferSize(optval);
    optval = Port->readBufferSize();
    if (optval != 0x100000)
    {
        qDebug() << "[create_optitrack_data_socket] ReceiveBuffer size = " << QString::number(optval);
    }
    else
    {
        qDebug() << "[create_optitrack_data_socket] Increased receive buffer size to " << QString::number(optval);
    }

    return true;
}

std::vector<optitrack_message_t> mocap_optitrack::parse_optitrack_packet_into_messages(
    const char* packet, int size) {
  // Prince: This variable is currently unused and causing a compiler warning
  // However, I don't want to change the function prototype
  size = size;

  std::vector<optitrack_message_t> messages;

  int major = 3;
  int minor = 0;

  const char* ptr = packet;
  if (VERBOSE) {
    printf("Begin Packet\n-------\n");
  }

  // message ID
  int MessageID = 0;
  memcpy(&MessageID, ptr, 2);
  ptr += 2;
  if (VERBOSE) {
    printf("Message ID : %d\n", MessageID);
  }

  // size
  int nBytes = 0;
  memcpy(&nBytes, ptr, 2);
  ptr += 2;
  if (VERBOSE) {
    printf("Byte count : %d\n", nBytes);
  }
  if (MessageID == NAT_FRAMEOFDATA) {  // FRAME OF MOCAP DATA packet
    // frame number
    int frameNumber = 0;
    memcpy(&frameNumber, ptr, 4);
    ptr += 4;
    if (VERBOSE) {
      printf("Frame # : %d\n", frameNumber);
    }

    // number of data sets (markersets, rigidbodies, etc)
    int nMarkerSets = 0;
    memcpy(&nMarkerSets, ptr, 4);
    ptr += 4;
    if (VERBOSE) {
      printf("Marker Set Count : %d\n", nMarkerSets);
    }

    for (int i = 0; i < nMarkerSets; i++) {
      // Markerset name
      char szName[256];
      strcpy(szName, ptr);
      int nDataBytes = (int)strlen(szName) + 1;
      ptr += nDataBytes;
      // printf("Model Name: %s\n", szName);

      // marker data
      int nMarkers = 0;
      memcpy(&nMarkers, ptr, 4);
      ptr += 4;
      // printf("Marker Count : %d\n", nMarkers);

      for (int j = 0; j < nMarkers; j++) {
        float x = 0;
        memcpy(&x, ptr, 4);
        ptr += 4;
        float y = 0;
        memcpy(&y, ptr, 4);
        ptr += 4;
        float z = 0;
        memcpy(&z, ptr, 4);
        ptr += 4;
        // printf("\tMarker %d : [x=%3.2f,y=%3.2f,z=%3.2f]\n",j,x,y,z);
      }
    }

    // unidentified markers
    int nOtherMarkers = 0;
    memcpy(&nOtherMarkers, ptr, 4);
    ptr += 4;
    if (VERBOSE) {
      printf("Unidentified Marker Count : %d\n", nOtherMarkers);
    }
    for (int j = 0; j < nOtherMarkers; j++) {
      float x = 0.0f;
      memcpy(&x, ptr, 4);
      ptr += 4;
      float y = 0.0f;
      memcpy(&y, ptr, 4);
      ptr += 4;
      float z = 0.0f;
      memcpy(&z, ptr, 4);
      ptr += 4;
      if (VERBOSE) {
        printf("\tMarker %d : pos = [%3.2f,%3.2f,%3.2f]\n", j, x, y, z);
      }
    }

    // rigid bodies
    int nRigidBodies = 0;
    memcpy(&nRigidBodies, ptr, 4);
    ptr += 4;
    if (VERBOSE) {
      printf("Rigid Body Count : %d\n", nRigidBodies);
    }
    for (int j = 0; j < nRigidBodies; j++) {
      optitrack_message_t msg;
      // rigid body position/orientation
      memcpy(&(msg.id), ptr, 4);
      ptr += 4;
      memcpy(&(msg.x), ptr, 4);
      ptr += 4;
      memcpy(&(msg.y), ptr, 4);
      ptr += 4;
      memcpy(&(msg.z), ptr, 4);
      ptr += 4;
      memcpy(&(msg.qx), ptr, 4);
      ptr += 4;
      memcpy(&(msg.qy), ptr, 4);
      ptr += 4;
      memcpy(&(msg.qz), ptr, 4);
      ptr += 4;
      memcpy(&(msg.qw), ptr, 4);
      ptr += 4;

      if (VERBOSE) {
        printf("ID : %d\n", msg.id);
        printf("pos: [%3.2f,%3.2f,%3.2f]\n", msg.x, msg.y, msg.z);
        printf("ori: [%3.2f,%3.2f,%3.2f,%3.2f]\n", msg.qx, msg.qy, msg.qz,
               msg.qw);
      }

      // NatNet version 2.0 and later
      if (major >= 2) {
        // Mean marker error
        float fError = 0.0f;
        memcpy(&fError, ptr, 4);
        ptr += 4;
      }

      // NatNet version 2.6 and later
      if (((major == 2) && (minor >= 6)) || (major > 2) || (major == 0)) {
        // params
        short params = 0;
        memcpy(&params, ptr, 2);
        ptr += 2;
        bool bTrackingValid =
            params &
            0x01;  // 0x01 : rigid body was successfully tracked in this frame
        msg.trackingValid = bTrackingValid;
      }
      messages.push_back(msg);

    }  // Go to next rigid body

  } else {
    printf("Unrecognized Packet Type: %d\n", MessageID);
  }

  return messages;
}

void mocap_optitrack::toEulerAngle(const optitrack_message_t& msg, double& roll, double& pitch,
                  double& yaw) {
  // from wikipedia quaternion entry
  double ysqr = msg.qy * msg.qy;

  // roll (x-axis rotation)
  double t0 = +2.0 * (msg.qw * msg.qx + msg.qy * msg.qz);
  double t1 = +1.0 - 2.0 * (msg.qx * msg.qx + ysqr);
  roll = std::atan2(t0, t1);

  // pitch (y-axis rotation)
  double t2 = +2.0 * (msg.qw * msg.qy - msg.qz * msg.qx);
  t2 = t2 > 1.0 ? 1.0 : t2;
  t2 = t2 < -1.0 ? -1.0 : t2;
  pitch = std::asin(t2);

  // yaw (z-axis rotation)
  double t3 = +2.0 * (msg.qw * msg.qz + msg.qx * msg.qy);
  double t4 = +1.0 - 2.0 * (ysqr + msg.qz * msg.qz);
  yaw = std::atan2(t3, t4);
}













//#define DEBUG

void __copy_data(mocap_data_t& buff_out, mocap_data_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    buff_out.roll = buff_in.roll;
    buff_out.pitch = buff_in.pitch;
    buff_out.yaw = buff_in.yaw;
    return;
}
void __copy_data(optitrack_message_t& buff_out, mocap_data_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    return;
}
void __copy_data(optitrack_message_t& buff_out, optitrack_message_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    return;
}
void __copy_data(mocap_data_t& buff_out, optitrack_message_t& buff_in)
{
    buff_out.id = buff_in.id;
    buff_out.trackingValid = buff_in.trackingValid;
    buff_out.qw = buff_in.qw;
    buff_out.qx = buff_in.qx;
    buff_out.qy = buff_in.qy;
    buff_out.qz = buff_in.qz;
    buff_out.x = buff_in.x;
    buff_out.y = buff_in.y;
    buff_out.z = buff_in.z;
    toEulerAngle(buff_in, buff_out.roll, buff_out.pitch, buff_out.yaw);
    return;
}

void __rotate_YUP2NED(optitrack_message_t& buff_in)
{
    optitrack_message_t tmp = buff_in;

    //buff_in.x = tmp.x;
    buff_in.y = tmp.z;
    buff_in.z = -tmp.y;

    //buff_in.qw = tmp.qw;
    //buff_in.qx = tmp.qx;
    buff_in.qy = tmp.qz;
    buff_in.qz = -tmp.qy;
    return;
}

void __rotate_ZUP2NED(optitrack_message_t& buff_in)
{
    optitrack_message_t tmp = buff_in;

    //buff_in.x = tmp.x;
    buff_in.y = -tmp.y;
    buff_in.z = -tmp.z;

    //buff_in.qw = tmp.qw;
    //buff_in.qx = tmp.qx;
    buff_in.qy = -tmp.qy;
    buff_in.qz = -tmp.qz;
    return;
}

/* check if MOCAP data is new */
bool is_mocap_data_same(mocap_data_t& data_new, mocap_data_t& data_old)
{
    if (data_new.pitch != data_old.pitch) return false;
    if (data_new.roll != data_old.roll) return false;
    if (data_new.yaw != data_old.yaw) return false;
    if (data_new.x != data_old.x) return false;
    if (data_new.y != data_old.y) return false;
    if (data_new.z != data_old.z) return false;
    return true;
}


// ------------------------------------------------------------------------------
//  Pthread Starter Helper Functions
// ------------------------------------------------------------------------------

void* start_mocap_read_thread(void* args)
{
    // takes an autopilot object argument
    mocap_node_t* autopilot_interface = (mocap_node_t*)args;

    // run the object's read thread
    autopilot_interface->start_read_thread();

    // done!
    return NULL;
}

// ------------------------------------------------------------------------------
//   Start MOCAP Thread
// ------------------------------------------------------------------------------
char mocap_node_t::start(std::string ip_addr)
{
    if (init(ip_addr) < 0)
    {
        printf("ERROR in start: failed to initialize ip\n");
        stop();
        return -1;
    }
    if (thread.init(MOCAP_THREAD_PRI, MOCAP_THREAD_TYPE))
    {
        printf("ERROR in start: failed to initialize thread\n");
        stop();
        return -1;
    }
    if (thread.start(&start_mocap_read_thread, this))
    {
        printf("ERROR in start: failed to start thread\n");
        stop();
        return -1;
    }

    return 0;
}

// ------------------------------------------------------------------------------
//   Stop MOCAP Thread
// ------------------------------------------------------------------------------
char mocap_node_t::stop(void)
{
#ifdef DEBUG
    printf("Terminating mocap node thread\n");
#endif // DEBUG

    // signal exit
    time_to_exit = true;

    // wait for exit
    thread.stop(MOCAP_THREAD_TOUT);
    //pthread_join(read_tid, NULL);

    // destroy mutex
    pthread_mutex_destroy(&lock);
#ifdef DEBUG
    printf("Done terminating mocap node thread\n");
#endif // DEBUG
    return 0;
}

// ------------------------------------------------------------------------------
//   Start Mocap Read Thread
// ------------------------------------------------------------------------------
void mocap_node_t::start_read_thread()
{
    if (reading_status != 0)
    {
        fprintf(stderr, "ERROR in start_read_thread: mocap reading thread is already running\n");
        return;
    }
    else
    {
        read_thread();
        return;
    }

}

// ------------------------------------------------------------------------------
//   Mocap Read Thread
// ------------------------------------------------------------------------------
void mocap_node_t::read_thread()
{
    reading_status = true;

    while (!time_to_exit)
    {
        read_messages();
        usleep(1.0E6 / MOCAP_THREAD_HZ); // Read batches at 100Hz
    }

    reading_status = false;

    return;
}

// ------------------------------------------------------------------------------
//   Initialize MOCAP Node
// ------------------------------------------------------------------------------
char mocap_node_t::init(std::string ip_addr)
{
    dataSocket = 0;

    // Start mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        fprintf(stderr,"ERROR in start: failed to initialize mutex\n");
        return -1;
    }

    if (ip_addr.length() == 0) {
        ip_addr = guess_optitrack_network_interface();
    }
    // If there's still no interface, we have a problem
    if (ip_addr.length() == 0) {
        fprintf(stderr,
                "[optitrack_driver] error could not determine network ip address for "
                "receiving multicast packets.\n");
        return -1;
    }

    dataSocket = create_optitrack_data_socket(ip_addr, PORT_DATA);

    if (dataSocket == -1) {
        fprintf(stderr,
                "ERROR in open: error failed to create socket for ip address "
                "%s:%d\n",
                ip_addr.c_str(), PORT_DATA);
        return -1;
    }
    else {
        fprintf(stderr,
                "Successfully created socket for ip address "
                "%s:%d\n",
                ip_addr.c_str(), PORT_DATA);
    }
    time_to_exit = false;
    return 0;
}

// ------------------------------------------------------------------------------
//   March MOCAP Node
// ------------------------------------------------------------------------------
char mocap_node_t::march(void)
{
    // Block until we receive a datagram from the network
    sockaddr_in incomingAddress;
    recvfrom(dataSocket, buff, BUFF_LEN, 0,
             (sockaddr*)&incomingAddress, &ADDRLEN);

    // Lock
    pthread_mutex_lock(&lock);
    incomingMessages =
        parse_optitrack_packet_into_messages(buff, BUFF_LEN);
    time_us_old = time_us;
    time_us = get_time_usec();
    // Unlock
    pthread_mutex_unlock(&lock);
    return 0;
}

// ------------------------------------------------------------------------------
//   Reads most recent data
// ------------------------------------------------------------------------------
char mocap_node_t::get_data(mocap_data_t& buff, int ID)
{
    // Lock
    pthread_mutex_lock(&lock);

    size_t tmp = incomingMessages.size();
    if (tmp < 1)
    {
#ifdef DEBUG
        printf("WARNING in get_data: have not received new data (vector is empty)\n");
#endif // DEBUG

        // Unlock
        pthread_mutex_unlock(&lock);
        return 0;
    }
    for (auto& msg : incomingMessages)
    {
        if (msg.id == ID)
        {
            optitrack_message_t tmp_msg = msg;
            if (YUP2END) __rotate_YUP2NED(tmp_msg);
            else if (ZUP2NED) __rotate_ZUP2NED(tmp_msg);
            __copy_data(buff, tmp_msg);
            buff.time_us_old = time_us_old;
            buff.time_us = time_us;

            // Unlock
            pthread_mutex_unlock(&lock);
            return 0;
        }
    }
    // Unlock
    pthread_mutex_unlock(&lock);
    printf("WARNING in get_data: no rigid body matching ID=%i\n", ID);
    return -1;
}


/* toggle coordinate frame rotation */
void mocap_node_t::togle_YUP2NED(bool in)
{
    if (in)
    {
        ZUP2NED = false;
        YUP2END = true;
    }
    else
    {
        YUP2END = false;
    }
    return;
}
void mocap_node_t::togle_ZUP2NED(bool in)
{
    if (in)
    {
        ZUP2NED = true;
        YUP2END = false;
    }
    else
    {
        ZUP2NED = false;
    }
    return;
}


// ------------------------------------------------------------------------------
//   Read MOCAP Messages
// ------------------------------------------------------------------------------
void mocap_node_t::read_messages()
{
    march();
    return;
}


mocap_node_t::mocap_node_t()
{
    dataSocket = 0;
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        fprintf(stderr, "ERROR in start: failed to initialize mutex\n");
        throw 1;
    }
}

mocap_node_t::~mocap_node_t()
{
    stop();
}
