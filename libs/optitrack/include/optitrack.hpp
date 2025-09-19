/****************************************************************************
 *
 *    Copyright (C) 2025  Yevhenii Kovryzhenko. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Affero General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License Version 3 for more details.
 *
 *    You should have received a copy of the
 *    GNU Affero General Public License Version 3
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions, and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. No ownership or credit shall be claimed by anyone not mentioned in
 *       the above copyright statement.
 *    4. Any redistribution or public use of this software, in whole or in part,
 *       whether standalone or as part of a different project, must remain
 *       under the terms of the GNU Affero General Public License Version 3,
 *       and all distributions in binary form must be accompanied by a copy of
 *       the source code, as stated in the GNU Affero General Public License.
 *
 ****************************************************************************/


#ifndef OPTITRACK_OPTITRACK_HPP
#define OPTITRACK_OPTITRACK_HPP

#include <QUdpSocket>
#include <QObject>
#include <QMutex>

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

    bool read_message(QVector<optitrack_message_t> &msg_out);

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
    // bool create_optitrack_data_socket_ipv6(\
    //                                        QNetworkInterface &interface, \
    //                                        QString local_address, unsigned short local_port,\
    //                                        QString multicast_address);


    /**
    * parse_optitrack_packet_into_messages parses the contents of a datagram received from the Optitrack server
    * into a vector containing the 3D position + quaternion for every rigid body
    */
    bool parse_optitrack_packet_into_messages(int &nBytes, std::vector<optitrack_message_t> &messages);
    bool is_ready_to_parse(void);

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
    QUdpSocket *Port = nullptr;
    QNetworkInterface* iface = nullptr;
    QMutex* mutex = nullptr;
    QString multicast_address_;

    void read_port(void);
    QByteArray bytearray;
};



#endif // OPTITRACK_OPTITRACK_HPP
