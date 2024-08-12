/****************************************************************************
//          Auburn University Aerospace Engineering Department
//             Aero-Astro Computational and Experimental lab
//
//     Copyright (C) 2024  Yevhenii Kovryzhenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the
//     GNU AFFERO GENERAL PUBLIC LICENSE Version 3
//     along with this program.  If not, see <https://www.gnu.org/licenses/>
//
****************************************************************************/

#ifndef MAVLINK_ENUM_TYPES_H
#define MAVLINK_ENUM_TYPES_H

#include <QMetaEnum>
#include "default_ui_config.h"

class mavlink_enums : public QObject
{
    Q_OBJECT
public:
    enum mavlink_component_id
    {
        ALL=0, /* Target id (target_component) used to broadcast messages to all components of the receiving system. Components should attempt to process messages with this component ID and forward to components on any other interfaces. Note: This is not a valid *source* component id for a message. | */
        AUTOPILOT1=1, /* System flight controller component ("autopilot"). Only one autopilot is expected in a particular system. | */
        USER1=25, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER2=26, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER3=27, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER4=28, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER5=29, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER6=30, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER7=31, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER8=32, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER9=33, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER10=34, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER11=35, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER12=36, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER13=37, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER14=38, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER15=39, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER16=40, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER17=41, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER18=42, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER19=43, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER20=44, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER21=45, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER22=46, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER23=47, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER24=48, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER25=49, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER26=50, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER27=51, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER28=52, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER29=53, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER30=54, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER31=55, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER32=56, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER33=57, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER34=58, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER35=59, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER36=60, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER37=61, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER38=62, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER39=63, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER40=64, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER41=65, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER42=66, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER43=67, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        TELEMETRY_RADIO=68, /* Telemetry radio (e.g. SiK radio, or other component that emits RADIO_STATUS messages). | */
        USER45=69, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER46=70, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER47=71, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER48=72, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER49=73, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER50=74, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER51=75, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER52=76, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER53=77, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER54=78, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER55=79, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER56=80, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER57=81, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER58=82, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER59=83, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER60=84, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER61=85, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER62=86, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER63=87, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER64=88, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER65=89, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER66=90, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER67=91, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER68=92, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER69=93, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER70=94, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER71=95, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER72=96, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER73=97, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER74=98, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        USER75=99, /* Id for a component on privately managed MAVLink network. Can be used for any purpose but may not be published by components outside of the private network. | */
        CAMERA=100, /* Camera #1. | */
        CAMERA2=101, /* Camera #2. | */
        CAMERA3=102, /* Camera #3. | */
        CAMERA4=103, /* Camera #4. | */
        CAMERA5=104, /* Camera #5. | */
        CAMERA6=105, /* Camera #6. | */
        SERVO1=140, /* Servo #1. | */
        SERVO2=141, /* Servo #2. | */
        SERVO3=142, /* Servo #3. | */
        SERVO4=143, /* Servo #4. | */
        SERVO5=144, /* Servo #5. | */
        SERVO6=145, /* Servo #6. | */
        SERVO7=146, /* Servo #7. | */
        SERVO8=147, /* Servo #8. | */
        SERVO9=148, /* Servo #9. | */
        SERVO10=149, /* Servo #10. | */
        SERVO11=150, /* Servo #11. | */
        SERVO12=151, /* Servo #12. | */
        SERVO13=152, /* Servo #13. | */
        SERVO14=153, /* Servo #14. | */
        GIMBAL=154, /* Gimbal #1. | */
        LOG=155, /* Logging component. | */
        ADSB=156, /* Automatic Dependent Surveillance-Broadcast (ADS-B) component. | */
        OSD=157, /* On Screen Display (OSD) devices for video links. | */
        PERIPHERAL=158, /* Generic autopilot peripheral component ID. Meant for devices that do not implement the parameter microservice. | */
        QX1_GIMBAL=159, /* Gimbal ID for QX1. | */
        FLARM=160, /* FLARM collision alert component. | */
        PARACHUTE=161, /* Parachute component. | */
        WINCH=169, /* Winch component. | */
        GIMBAL2=171, /* Gimbal #2. | */
        GIMBAL3=172, /* Gimbal #3. | */
        GIMBAL4=173, /* Gimbal #4 | */
        GIMBAL5=174, /* Gimbal #5. | */
        GIMBAL6=175, /* Gimbal #6. | */
        BATTERY=180, /* Battery #1. | */
        BATTERY2=181, /* Battery #2. | */
        MAVCAN=189, /* CAN over MAVLink client. | */
        MISSIONPLANNER=190, /* Component that can generate/supply a mission flight plan (e.g. GCS or developer API). | */
        ONBOARD_COMPUTER=191, /* Component that lives on the onboard computer (companion computer) and has some generic functionalities, such as settings system parameters and monitoring the status of some processes that don't directly speak mavlink and so on. | */
        ONBOARD_COMPUTER2=192, /* Component that lives on the onboard computer (companion computer) and has some generic functionalities, such as settings system parameters and monitoring the status of some processes that don't directly speak mavlink and so on. | */
        ONBOARD_COMPUTER3=193, /* Component that lives on the onboard computer (companion computer) and has some generic functionalities, such as settings system parameters and monitoring the status of some processes that don't directly speak mavlink and so on. | */
        ONBOARD_COMPUTER4=194, /* Component that lives on the onboard computer (companion computer) and has some generic functionalities, such as settings system parameters and monitoring the status of some processes that don't directly speak mavlink and so on. | */
        PATHPLANNER=195, /* Component that finds an optimal path between points based on a certain constraint (e.g. minimum snap, shortest path, cost, etc.). | */
        OBSTACLE_AVOIDANCE=196, /* Component that plans a collision free path between two points. | */
        VISUAL_INERTIAL_ODOMETRY=197, /* Component that provides position estimates using VIO techniques. | */
        PAIRING_MANAGER=198, /* Component that manages pairing of vehicle and GCS. | */
        IMU=200, /* Inertial Measurement Unit (IMU) #1. | */
        IMU_2=201, /* Inertial Measurement Unit (IMU) #2. | */
        IMU_3=202, /* Inertial Measurement Unit (IMU) #3. | */
        GPS=220, /* GPS #1. | */
        GPS2=221, /* GPS #2. | */
        ODID_TXRX_1=236, /* Open Drone ID transmitter/receiver (Bluetooth/WiFi/Internet). | */
        ODID_TXRX_2=237, /* Open Drone ID transmitter/receiver (Bluetooth/WiFi/Internet). | */
        ODID_TXRX_3=238, /* Open Drone ID transmitter/receiver (Bluetooth/WiFi/Internet). | */
        UDP_BRIDGE=240, /* Component to bridge MAVLink to UDP (i.e. from a UART). | */
        UART_BRIDGE=241, /* Component to bridge to UART (i.e. from UDP). | */
        TUNNEL_NODE=242, /* Component handling TUNNEL messages (e.g. vendor specific GUI of a component). | */
        SYSTEM_CONTROL=250, /* Deprecated, don't use. Component for handling system messages (e.g. to ARM, takeoff, etc.). | */
        MAV_COMPONENT_ENUM_END=251, /*  | */
    };
    Q_ENUM(mavlink_component_id)
    // static bool get_compid(mavlink_component_id &id_out, QString comp_id_string_in);

    static bool get_compid(mavlink_component_id &id_out, QString comp_id_string_in)
    {
        return enum_helpers::key2value(comp_id_string_in, id_out);
    }
};

#endif // MAVLINK_ENUM_TYPES_H
