#pragma once

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

#define MAVLINK_USE_MESSAGE_INFO
#include "all/mavlink.h"
#include "settings.h"

class log_manager {
public:
    enum class io_direction : uint8_t {
        incoming = 0,
        outgoing = 1
    };

    static log_manager& instance();

    static QString default_log_directory();

    void apply_settings(const kgroundcontrol_settings& settings);

    // Log one fully parsed inbound MAVLink message.
    void log_incoming_message(const QString& port_name, const mavlink_message_t& message);

    // Log one fully parsed outbound MAVLink message.
    void log_outgoing_message(const QString& port_name, const mavlink_message_t& message);

    // Log outbound raw bytes (used by write_to_port); parser reassembles MAVLink frames.
    void log_outgoing_bytes(const QString& port_name, const QByteArray& bytes);

    void shutdown();

    struct packet_record {
        uint64_t timestamp_us = 0;
        uint8_t direction_value = static_cast<uint8_t>(io_direction::incoming);
        QString port_name;
        QString topic_name;
        mavlink_message_t message{};
    };

private:
    log_manager();
    ~log_manager();

    log_manager(const log_manager&) = delete;
    log_manager& operator=(const log_manager&) = delete;

    static packet_record make_record(io_direction dir, const QString& port_name, const mavlink_message_t& message);
    void enqueue_record(packet_record&& record);

    void writer_loop();

    // Outgoing parser state (for raw byte streams from write_to_port)
    QMutex outgoing_parse_mutex_;
    QHash<QString, mavlink_status_t> outgoing_parse_status_;

    std::atomic_bool enabled_fast_{false};

    std::mutex state_mutex_;
    std::condition_variable state_cv_;
    std::deque<packet_record> queue_;

    bool stop_requested_ = false;
    bool config_dirty_ = true;
    bool rotate_requested_ = false;
    bool enabled_config_ = false;
    QString directory_;

    std::thread writer_thread_;
};
