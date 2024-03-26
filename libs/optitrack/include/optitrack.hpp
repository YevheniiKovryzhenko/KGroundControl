//================================================================================
// NOTE: This code is adapted from PacketClient.cpp, which remains in this folder.
// The license for the code is located in that file and duplicated below:
//
//=============================================================================
// Copyright © 2014 NaturalPoint, Inc. All Rights Reserved.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall NaturalPoint, Inc. or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//=============================================================================


#ifndef OPTITRACK_OPTITRACK_HPP
#define OPTITRACK_OPTITRACK_HPP

#include <vector>
#include <QUdpSocket>
#include <QObject>
#include <QMutex>

#define SOCKET int  // A sock handle is just an int in linux

#define MULTICAST_ADDRESS   "239.255.42.99"     // IANA, local network
#define MULTICAST_ADDRESS_6 "0:0:0:0:0:FFFF:EFFF:2A63"	// ipv6 of above
#define PORT_COMMAND        1510
#define PORT_DATA           1511

/**
* optitrack_message_t defines the data for a rigid body transmitted by the Optitrack server. A rigid body has a unique
* id, a 3D position, and a quaternion defining the orientation.
*/
struct optitrack_message_t
{
    int id;     ///< Unique id for the rigid body being described
    float x;    ///< x-position in the Optitrack frame
    float y;    ///< y-position in the Optitrack frame
    float z;    ///< z-position in the Optitrack frame
    float qx;   ///< qx of quaternion
    float qy;   ///< qy of quaternion
    float qz;   ///< qz of quaternion
    float qw;   ///< qw of quaternion
    bool trackingValid;     // Whether or not tracking was valid for the rigid body
};

class mocap_optitrack : public QObject
{
    Q_OBJECT

public:
    explicit mocap_optitrack(QObject *parent = nullptr);
    ~mocap_optitrack();

    bool read(std::vector<optitrack_message_t> &msg_out);
    bool read_ipv6(std::vector<optitrack_message_t> &msg_out);



    /**
    * create_optitrack_data_socket creates a socket for receiving Optitrack data from the Optitrack server.
    *
    * The data socket will receive data on the predefined MULTICAST_ADDRESS.
    *
    * \param    interfaceIp         IP address of the interface that will be receiving Optitrack data
    * \param    port                Port to which optitrack data is being sent
    * \return   SOCKET handle to use for receiving data from the Optitrack server.
    */
    bool create_optitrack_data_socket(\
                                      QNetworkInterface &interface, \
                                      QString local_address, unsigned short local_port,\
                                      QString multicast_address);

    /**
    *
    *	Same as above but for IPV6
    *
    * create_optitrack_data_socket creates a socket for receiving Optitrack data from the Optitrack server.
    *
    * The data socket will receive data on the predefined MULTICAST_ADDRESS_6.
    *
    * \param    interfaceIp         IP address of the interface that will be receiving Optitrack data
    * \param    port                Port to which optitrack data is being sent
    * \return   SOCKET handle to use for receiving data from the Optitrack server.
    */
    bool create_optitrack_data_socket_ipv6(\
                                           QNetworkInterface &interface, \
                                           QString local_address, unsigned short local_port,\
                                           QString multicast_address);


    /**
    * parse_optitrack_packet_into_messages parses the contents of a datagram received from the Optitrack server
    * into a vector containing the 3D position + quaternion for every rigid body
    */
    static std::vector<optitrack_message_t> parse_optitrack_packet_into_messages(const char* packet);

    /**
    * guess_optitrack_network_interface tries to find the IP address of the interface to use for receiving Optitrack data.
    * The strategy used will:
    *
    *   1) Enumerate all possible network interfaces.
    *   2) Ignore 'lo'
    *   3) Either:
    *       - Find the first interface that starts with the letter "w", which most likely indicates a wireless network.
    *       - Or, return any interface found if no interface starts with "w".
    *
    * \return   IP address of the multicast interface to use with optitrack
    */
    static bool guess_optitrack_network_interface(QNetworkInterface &interface);
    static bool guess_optitrack_network_interface_ipv6(QNetworkInterface &interface);

    static bool get_network_interface(QNetworkInterface &interface, QString address);

    /**
    *
    *
    * \param    msg         Optitrack message containing the rigid body quaternion
    * \param    roll        return roll
    * \param    pitch       return pitch
    * \param    yaw         return yaw
    */
    static void toEulerAngle(const optitrack_message_t& msg, double& roll, double& pitch, double& yaw);

private:
    bool port_open = false;
    QUdpSocket *Port = nullptr;
    QNetworkInterface* iface = nullptr;
    QMutex* mutex = nullptr;
    QString multicast_address_;

    bool port_ipv6_open = false;
    QUdpSocket *Port_ipv6 = nullptr;
    QNetworkInterface* iface_ipv6 = nullptr;
    QMutex* mutex_ipv6 = nullptr;
    QString multicast_address_ipv6_;

    const static int BUFF_LEN = 20000;
    char buff[BUFF_LEN];
    int buff_index = 0;
    char buff_ipv6[BUFF_LEN];
    int buff_ipv6_index = 0;
    // QString* iface_address = nullptr;
};



#endif // OPTITRACK_OPTITRACK_HPP
