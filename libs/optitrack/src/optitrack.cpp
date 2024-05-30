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
#include <QHostInfo>
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
        Port->close();
        delete Port;
        Port = nullptr;
    }
    delete mutex;
}

void mocap_optitrack::read_port(void)
{
    mutex->lock();
    QNetworkDatagram datagram = Port->receiveDatagram();
    QByteArray new_data = datagram.data();
    bytearray.append(new_data);
    mutex->unlock();
}

bool mocap_optitrack::read_message(std::vector<optitrack_message_t> &msg_out)
{
    bool msgReceived = false;

    mutex->lock();
    if (is_ready_to_parse())
    {
        std::vector<optitrack_message_t> parsed_data;
        int nbytes = parse_optitrack_packet_into_messages(parsed_data);

        if (parsed_data.size() > 0)
        {
            msg_out.insert(std::end(msg_out), std::begin(parsed_data), std::end(parsed_data));
            msgReceived = true;
        }
        if (nbytes < bytearray.size()) bytearray.remove(0, nbytes); //keep data for next parsing run
        else bytearray.clear(); //start fresh next time
    }
    mutex->unlock();

    // Done!
    return msgReceived;
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
    auto tmp0 = QNetworkInterface::allInterfaces();
    foreach (const QNetworkInterface &interface_, QNetworkInterface::allInterfaces())
    {
        if (interface_.flags().testFlag(QNetworkInterface::IsUp))
        {
            auto tmp1 = interface_.allAddresses();
            foreach(const QHostAddress &address_, interface_.allAddresses())
            {
                if (address_.toString() == address)
                {
                    interface = interface_;
                    return true;
                }
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
        Port->close();
        delete Port;
        Port = nullptr;
    }
    Port = new QUdpSocket();

    if (!Port->bind(QHostAddress(local_address), local_port, QUdpSocket::ShareAddress))
    {
        (new QErrorMessage)->showMessage(Port->errorString());
        mutex->unlock();
        return false;
    }
    if (Port->joinMulticastGroup(QHostAddress(multicast_address), interface))
    {
        qDebug() << "[create_optitrack_data_socket] joined multicast group at address " << QString(MULTICAST_ADDRESS);
        iface = new QNetworkInterface(interface);
    }
    else
    {
        (new QErrorMessage)->showMessage("[create_optitrack_data_socket] join failed.\n");
        Port->close();
        delete Port;
        Port = nullptr;
        mutex->unlock();
        return false;
    }
    multicast_address_ = multicast_address;
    // create a 1MB buffer
    int optval = 0x100000;

    Port->setReadBufferSize(optval);
    optval = Port->readBufferSize();
    if (optval != 0x100000) qDebug() << "[create_optitrack_data_socket] ReceiveBuffer size = " << QString::number(optval);
    else qDebug() << "[create_optitrack_data_socket] Increased receive buffer size to " << QString::number(optval);

    mutex->unlock();

    connect(Port, &QUdpSocket::readyRead, this, &mocap_optitrack::read_port, Qt::DirectConnection);
    return true;
}

bool mocap_optitrack::is_ready_to_parse(void)
{

    if (bytearray.size() < 5) return false;
    else
    {
        int MessageID;
        int nBytes = 0;
        while (bytearray.size() > 4) {
            char* ptr = bytearray.begin();

            // message ID
            MessageID = 0;
            memcpy(&MessageID, ptr, 2);
            if (MessageID != NAT_FRAMEOFDATA)
            {
                bytearray.remove(0, 2);
                continue;
            }

            if (bytearray.size() < 5) return false;
            ptr += 2;

            //size
            nBytes = 0;
            memcpy(&nBytes, ptr, 2);
            if (nBytes < 1) bytearray.remove(0, 4);
            else break;
        }

        return (nBytes > 0 && bytearray.size() >= nBytes);
    }
}

bool check_size(int max, int requested)
{
    if (max < requested)
    {
        qDebug() << "[mocap_optitrack] Requesting more data than received, pausing data parsing...";
        return true;
    }
    return false;
}
int mocap_optitrack::parse_optitrack_packet_into_messages(std::vector<optitrack_message_t> &messages)
{

  // std::vector<optitrack_message_t> messages;

  int major = 3;
  int minor = 0;  

  char* ptr = bytearray.begin();
  int n_bytes_read = 0;
  if (VERBOSE) {
      qDebug() << "Begin Packet\n-------\n";
  }

  // message ID
  int MessageID = 0;
  memcpy(&MessageID, ptr, 2);
  ptr += 2;
  if (VERBOSE) {
    qDebug() << "Message ID : " << MessageID;
  }

  // size
  int nBytes = 0;
  memcpy(&nBytes, ptr, 2);
  ptr += 2;
  if (VERBOSE) {
    qDebug() << "Byte count : " << nBytes;
  }
  n_bytes_read = 4;
  if (nBytes < 5) return n_bytes_read;
  if (nBytes > bytearray.size())
  {
      qDebug() << "[mocap_optitrack] Need to wait for more data before parsing..."; //should not occur
      return 0;
  }
  if (MessageID == NAT_FRAMEOFDATA) {  // FRAME OF MOCAP DATA packet
    // frame number
    int frameNumber = 0;
    if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
    n_bytes_read += 4;
    memcpy(&frameNumber, ptr, 4);
    ptr += 4; //8
    if (VERBOSE) {
      qDebug() << "Frame # : " << frameNumber;
    }

    // number of data sets (markersets, rigidbodies, etc)
    int nMarkerSets = 0;
    if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
    n_bytes_read += 4;
    memcpy(&nMarkerSets, ptr, 4);
    ptr += 4; //12
    if (VERBOSE) {
      qDebug() << "Marker Set Count : " << nMarkerSets;
    }
    for (int i = 0; i < nMarkerSets; i++) {
      // Markerset name
      char szName[256];
      strcpy(szName, ptr);
      int nDataBytes = (int)strlen(szName) + 1;
      if (check_size(nBytes, n_bytes_read + nDataBytes)) return n_bytes_read;
      n_bytes_read += nDataBytes;
      ptr += nDataBytes;
      // printf("Model Name: %s\n", szName);

      // marker data
      int nMarkers = 0;
      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&nMarkers, ptr, 4);
      ptr += 4;
      // printf("Marker Count : %d\n", nMarkers);

      for (int j = 0; j < nMarkers; j++) {
        float x = 0;
        if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
        n_bytes_read += 4;
        memcpy(&x, ptr, 4);
        ptr += 4;

        float y = 0;
        if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
        n_bytes_read += 4;
        memcpy(&y, ptr, 4);
        ptr += 4;

        float z = 0;
        if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
        n_bytes_read += 4;
        memcpy(&z, ptr, 4);
        ptr += 4;
        // printf("\tMarker %d : [x=%3.2f,y=%3.2f,z=%3.2f]\n",j,x,y,z);
      }
    }

    // unidentified markers
    int nOtherMarkers = 0;
    if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
    n_bytes_read += 4;
    memcpy(&nOtherMarkers, ptr, 4);
    ptr += 4;
    if (VERBOSE) {
      qDebug() << "Unidentified Marker Count : " << nOtherMarkers;
    }
    for (int j = 0; j < nOtherMarkers; j++) {
      float x = 0.0f;
        if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
        n_bytes_read += 4;
        memcpy(&x, ptr, 4);
        ptr += 4;

        float y = 0.0f;
        if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
        n_bytes_read += 4;
        memcpy(&y, ptr, 4);
        ptr += 4;

        float z = 0.0f;
        if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
        n_bytes_read += 4;
        memcpy(&z, ptr, 4);
        ptr += 4;
        if (VERBOSE) {
          qDebug() << "\tMarker " << j <<" : pos = [" << x << y << z << "]";
        }
    }

    // rigid bodies
    int nRigidBodies = 0;
    if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
    n_bytes_read += 4;
    memcpy(&nRigidBodies, ptr, 4);
    ptr += 4;
    if (VERBOSE) {
      qDebug() << "Rigid Body Count : " << nRigidBodies;
    }

    for (int j = 0; j < nRigidBodies; j++) {
      optitrack_message_t msg;
      // rigid body position/orientation
      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.id), ptr, 4);
      ptr += 4;

      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.x), ptr, 4);
      ptr += 4;

      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.y), ptr, 4);
      ptr += 4;

      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.z), ptr, 4);
      ptr += 4;

      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.qx), ptr, 4);
      ptr += 4;

      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.qy), ptr, 4);
      ptr += 4;

      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.qz), ptr, 4);
      ptr += 4;

      if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
      n_bytes_read += 4;
      memcpy(&(msg.qw), ptr, 4);
      ptr += 4;

      if (VERBOSE) {
            qDebug() << "ID : " << msg.id;
            qDebug() << "pos: [" << msg.x << msg.y << msg.z << "]";
            qDebug() << "ori: [" << msg.qx << msg.qy << msg.qz << msg.qw << "]\n";
      }

      // NatNet version 2.0 and later
      if (major >= 2) {
        // Mean marker error
        float fError = 0.0f;
        if (check_size(nBytes, n_bytes_read + 4)) return n_bytes_read;
        n_bytes_read += 4;
        memcpy(&fError, ptr, 4);
        ptr += 4;
      }

      // NatNet version 2.6 and later
      if (((major == 2) && (minor >= 6)) || (major > 2) || (major == 0)) {
        // params
        short params = 0;
        if (check_size(nBytes, n_bytes_read + 2)) return n_bytes_read;
        n_bytes_read += 2;
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
    qDebug() << "Unrecognized Packet Type: " << MessageID;
  }

  return n_bytes_read;
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
