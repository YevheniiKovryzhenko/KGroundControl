#ifndef GENERIC_PORT_H
#define GENERIC_PORT_H

/*
 * Generic Port Class
 *
 * This is an abstract port definition to handle both serial and UDP ports.
 */
class Generic_Port
{
public:
    Generic_Port(){};
    virtual ~Generic_Port(){};
    // virtual char read_message(mavlink_message_t &message, mavlink_channel_t mavlink_channel_)=0;
    // virtual int write_message(const mavlink_message_t &message)=0;
    virtual bool is_running()=0;
    virtual char start()=0;
    virtual void stop()=0;
    virtual int bytes_available(void) = 0;
};

#endif // GENERIC_PORT_H
