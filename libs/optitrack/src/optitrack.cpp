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

mocap_optitrack::mocap_optitrack(QObject *parent) : QObject(parent)
{
    mutex = new QMutex();
    port_open = false;

    mutex_ipv6 = new QMutex();
    port_ipv6_open = false;

    buff_index = 0;
    buff_ipv6_index = 0;
}

mocap_optitrack::~mocap_optitrack()
{
    if (Port != NULL)
    {
        if (iface != NULL)
        {
            Port->leaveMulticastGroup(QHostAddress(multicast_address_), *iface);
            delete iface;
            iface = nullptr;
        }
        delete Port;
        Port = nullptr;
    }
    delete mutex;
    port_open = false;


    if (Port_ipv6 != NULL)
    {
        if (iface_ipv6 != NULL)
        {
            Port_ipv6->leaveMulticastGroup(QHostAddress(multicast_address_ipv6_), *iface_ipv6);
            delete iface_ipv6;
            iface_ipv6 = nullptr;
        }
        delete Port_ipv6;
        Port_ipv6 = nullptr;
    }
    delete mutex_ipv6;
    port_ipv6_open = false;
}

bool mocap_optitrack::read(std::vector<optitrack_message_t> &msg_out)
{
    if (!port_open) return false;
    bool res = false;

    mutex->lock();
    if (Port->hasPendingDatagrams())
    {
        QNetworkDatagram datagram;
        datagram = Port->receiveDatagram();
        QByteArray data = datagram.data();
        foreach (auto byte, data)
        {
            buff[buff_index] = static_cast<char>(byte);
            buff_index++;

            if (buff_index > BUFF_LEN)
            {
                std::vector<optitrack_message_t> parsed_data = parse_optitrack_packet_into_messages(buff);
                msg_out.insert(std::end(msg_out), std::begin(parsed_data), std::end(parsed_data));
                res = true;

                buff_index = 0;
            }
        }
    }
    mutex->unlock();
    return res;
}

bool mocap_optitrack::read_ipv6(std::vector<optitrack_message_t> &msg_out)
{
    if (!port_ipv6_open) return false;
    bool res = false;

    mutex_ipv6->lock();
    if (Port_ipv6->hasPendingDatagrams())
    {
        QNetworkDatagram datagram;
        datagram = Port_ipv6->receiveDatagram();
        QByteArray data = datagram.data();
        foreach (auto byte, data)
        {
            buff_ipv6[buff_ipv6_index] = static_cast<char>(byte);
            buff_ipv6_index++;

            if (buff_ipv6_index > BUFF_LEN)
            {
                std::vector<optitrack_message_t> parsed_data = parse_optitrack_packet_into_messages(buff_ipv6);
                msg_out.insert(std::end(msg_out), std::begin(parsed_data), std::end(parsed_data));
                res = true;

                buff_ipv6_index = 0;
            }
        }
    }
    mutex_ipv6->unlock();
    return res;
}

bool mocap_optitrack::guess_optitrack_network_interface(QNetworkInterface &interface)
{
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

bool mocap_optitrack::guess_optitrack_network_interface_ipv6(QNetworkInterface &interface)
{
    //get a list of all network interfaces and ip addresses
    foreach (const QNetworkInterface &interface_, QNetworkInterface::allInterfaces())
    {
        QString interface_name = interface_.name();
        foreach(const QHostAddress &address, interface_.allAddresses())
        {

            if (address.protocol() == QAbstractSocket::IPv6Protocol)
            {
                // If the name starts with a 'w', then it is probably a wireless device
                if (interface_name[0] == 'w')
                {
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

bool mocap_optitrack::get_network_interface(QNetworkInterface &interface, QString address)
{
    //get a list of all network interfaces and ip addresses
    foreach (const QNetworkInterface &interface_, QNetworkInterface::allInterfaces())
    {
        foreach(const QHostAddress &address_, interface_.allAddresses())
        {
            if (address_.toString() == address)
            {
                interface = interface_;
                return true;
            }
        }
    }
    qDebug() << "[get_network_interface] failed to match interface with address";
    return false;
}

bool mocap_optitrack::create_optitrack_data_socket(\
            QNetworkInterface &interface, \
            QString local_address, unsigned short local_port,\
            QString multicast_address) {
    mutex->lock();
    if (Port != NULL)
    {
        if (iface != NULL)
        {
            Port->leaveMulticastGroup(QHostAddress(multicast_address_), *iface);
            delete iface;
            iface = nullptr;
        }
        delete Port;
        Port = nullptr;
    }
    Port = new QUdpSocket();

    if (!Port->bind(QHostAddress(local_address), local_port, QUdpSocket::ShareAddress))
    {
        (new QErrorMessage)->showMessage(Port->errorString());
        port_open = false;
        mutex->unlock();
        return port_open;
    }


    if (Port->joinMulticastGroup(QHostAddress(multicast_address), interface))
    {
        qDebug() << "[create_optitrack_data_socket] joined multicast group at address " << QString(MULTICAST_ADDRESS);
    }
    else
    {
        (new QErrorMessage)->showMessage("[create_optitrack_data_socket] join failed.\n");
        port_open = false;
        mutex->unlock();
        return port_open;
    }
    multicast_address_ = multicast_address;
    // create a 1MB buffer
    int optval = 0x100000;

    Port->setReadBufferSize(optval);
    optval = Port->readBufferSize();
    if (optval != 0x100000) qDebug() << "[create_optitrack_data_socket] ReceiveBuffer size = " << QString::number(optval);
    else qDebug() << "[create_optitrack_data_socket] Increased receive buffer size to " << QString::number(optval);

    port_open = true;
    buff_index = 0;
    mutex->unlock();
    return port_open;
}

bool mocap_optitrack::create_optitrack_data_socket_ipv6(\
            QNetworkInterface &interface, \
            QString local_address, unsigned short local_port,\
            QString multicast_address)
{
    mutex_ipv6->lock();
    if (Port_ipv6 != NULL)
    {
        if (iface_ipv6 != NULL)
        {
            Port_ipv6->leaveMulticastGroup(QHostAddress(multicast_address_ipv6_), *iface_ipv6);
            delete iface_ipv6;
            iface_ipv6 = nullptr;
        }
        delete Port_ipv6;
        Port_ipv6 = nullptr;
    }
    Port_ipv6 = new QUdpSocket();

    if (!Port_ipv6->bind(QHostAddress(local_address), local_port, QUdpSocket::ShareAddress))
    {
        (new QErrorMessage)->showMessage(Port_ipv6->errorString());
        port_ipv6_open = false;
        mutex_ipv6->unlock();
        return port_ipv6_open;
    }


    if (Port_ipv6->joinMulticastGroup(QHostAddress(multicast_address), interface))
    {
        qDebug() << "[create_optitrack_data_socket_ipv6] joined multicast group at address " << QString(MULTICAST_ADDRESS);
    }
    else
    {
        (new QErrorMessage)->showMessage("[create_optitrack_data_socket_ipv6] join failed.\n");
        port_ipv6_open = false;
        mutex_ipv6->unlock();
        return port_ipv6_open;
    }
    multicast_address_ipv6_ = multicast_address;
    // create a 1MB buffer
    int optval = 0x100000;

    Port_ipv6->setReadBufferSize(optval);
    optval = Port_ipv6->readBufferSize();
    if (optval != 0x100000) qDebug() << "[create_optitrack_data_socket_ipv6] ReceiveBuffer size = " << QString::number(optval);
    else qDebug() << "[create_optitrack_data_socket_ipv6] Increased receive buffer size to " << QString::number(optval);

    port_ipv6_open = true;
    buff_ipv6_index = 0;

    mutex_ipv6->unlock();
    return port_ipv6_open;
}

std::vector<optitrack_message_t> mocap_optitrack::parse_optitrack_packet_into_messages(
    const char* packet)
{

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
