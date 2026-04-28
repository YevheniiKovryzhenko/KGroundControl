#include "logging/log_manager.h"

#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVector>

#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <vector>
#include <ulog_cpp/writer.hpp>

namespace {

const char* ulog_type_for_mav_type(const mavlink_message_type_t type)
{
    switch (type) {
    case MAVLINK_TYPE_CHAR: return "char";
    case MAVLINK_TYPE_UINT8_T: return "uint8_t";
    case MAVLINK_TYPE_INT8_T: return "int8_t";
    case MAVLINK_TYPE_UINT16_T: return "uint16_t";
    case MAVLINK_TYPE_INT16_T: return "int16_t";
    case MAVLINK_TYPE_UINT32_T: return "uint32_t";
    case MAVLINK_TYPE_INT32_T: return "int32_t";
    case MAVLINK_TYPE_UINT64_T: return "uint64_t";
    case MAVLINK_TYPE_INT64_T: return "int64_t";
    case MAVLINK_TYPE_FLOAT: return "float";
    case MAVLINK_TYPE_DOUBLE: return "double";
    default: return nullptr;
    }
}

int mavlink_type_size_bytes(const mavlink_message_type_t type)
{
    switch (type) {
    case MAVLINK_TYPE_CHAR:
    case MAVLINK_TYPE_UINT8_T:
    case MAVLINK_TYPE_INT8_T:
        return 1;
    case MAVLINK_TYPE_UINT16_T:
    case MAVLINK_TYPE_INT16_T:
        return 2;
    case MAVLINK_TYPE_UINT32_T:
    case MAVLINK_TYPE_INT32_T:
    case MAVLINK_TYPE_FLOAT:
        return 4;
    case MAVLINK_TYPE_UINT64_T:
    case MAVLINK_TYPE_INT64_T:
    case MAVLINK_TYPE_DOUBLE:
        return 8;
    default:
        return 0;
    }
}

bool append_scalar_mavlink_value(QDataStream& ds,
                                 const mavlink_message_t& message,
                                 const mavlink_field_info_t& field,
                                 const unsigned int idx)
{
    const int element_size = mavlink_type_size_bytes(field.type);
    if (element_size <= 0) {
        return false;
    }

    const unsigned int raw_offset = field.wire_offset + idx * static_cast<unsigned int>(element_size);
    const uint8_t offset = static_cast<uint8_t>(raw_offset);

    switch (field.type) {
    case MAVLINK_TYPE_CHAR:
        ds << static_cast<qint8>(_MAV_RETURN_char(&message, offset));
        return true;
    case MAVLINK_TYPE_UINT8_T:
        ds << static_cast<quint8>(_MAV_RETURN_uint8_t(&message, offset));
        return true;
    case MAVLINK_TYPE_INT8_T:
        ds << static_cast<qint8>(_MAV_RETURN_int8_t(&message, offset));
        return true;
    case MAVLINK_TYPE_UINT16_T:
        ds << static_cast<quint16>(_MAV_RETURN_uint16_t(&message, offset));
        return true;
    case MAVLINK_TYPE_INT16_T:
        ds << static_cast<qint16>(_MAV_RETURN_int16_t(&message, offset));
        return true;
    case MAVLINK_TYPE_UINT32_T:
        ds << static_cast<quint32>(_MAV_RETURN_uint32_t(&message, offset));
        return true;
    case MAVLINK_TYPE_INT32_T:
        ds << static_cast<qint32>(_MAV_RETURN_int32_t(&message, offset));
        return true;
    case MAVLINK_TYPE_UINT64_T:
        ds << static_cast<quint64>(_MAV_RETURN_uint64_t(&message, offset));
        return true;
    case MAVLINK_TYPE_INT64_T:
        ds << static_cast<qint64>(_MAV_RETURN_int64_t(&message, offset));
        return true;
    case MAVLINK_TYPE_FLOAT:
    {
        const QDataStream::FloatingPointPrecision previous_precision = ds.floatingPointPrecision();
        ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
        ds << static_cast<float>(_MAV_RETURN_float(&message, offset));
        ds.setFloatingPointPrecision(previous_precision);
        return true;
    }
    case MAVLINK_TYPE_DOUBLE:
    {
        const QDataStream::FloatingPointPrecision previous_precision = ds.floatingPointPrecision();
        ds.setFloatingPointPrecision(QDataStream::DoublePrecision);
        ds << static_cast<double>(_MAV_RETURN_double(&message, offset));
        ds.setFloatingPointPrecision(previous_precision);
        return true;
    }
    default:
        return false;
    }
}

bool append_mavlink_field_value(QDataStream& ds,
                                const mavlink_message_t& message,
                                const mavlink_field_info_t& field)
{
    if (field.array_length == 0) {
        return append_scalar_mavlink_value(ds, message, field, 0);
    }

    if (field.type == MAVLINK_TYPE_CHAR) {
        QByteArray value(static_cast<int>(field.array_length), '\0');
        _MAV_RETURN_char_array(&message,
                               value.data(),
                               static_cast<uint8_t>(field.array_length),
                               static_cast<uint8_t>(field.wire_offset));
        return ds.writeRawData(value.constData(), value.size()) == value.size();
    }

    for (unsigned int idx = 0; idx < field.array_length; ++idx) {
        if (!append_scalar_mavlink_value(ds, message, field, idx)) {
            return false;
        }
    }

    return true;
}

QString direction_label(uint8_t direction_value)
{
    return (direction_value == static_cast<uint8_t>(log_manager::io_direction::incoming))
        ? QStringLiteral("input")
        : QStringLiteral("output");
}

QString sanitize_topic_component(const QString& input)
{
    QString out;
    out.reserve(input.size());

    bool last_was_underscore = false;
    for (const QChar ch : input) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_')) {
            out.append(ch);
            last_was_underscore = false;
            continue;
        }

        if (!last_was_underscore) {
            out.append(QLatin1Char('_'));
            last_was_underscore = true;
        }
    }

    while (!out.isEmpty() && out.front() == QLatin1Char('_')) {
        out.remove(0, 1);
    }
    while (!out.isEmpty() && out.back() == QLatin1Char('_')) {
        out.chop(1);
    }

    if (out.isEmpty()) {
        out = QStringLiteral("port");
    }
    if (out.front().isDigit()) {
        out.prepend(QStringLiteral("port_"));
    }
    if (out.size() > 40) {
        out = out.left(40);
    }

    return out;
}

class ulog_stream_writer {
public:
    bool open(const QString& file_path)
    {
        close();

        file_.setFileName(file_path);
        if (!file_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }

        writer_ = std::make_unique<ulog_cpp::Writer>([this](const uint8_t* data, int length) {
            if (length > 0) {
                file_.write(reinterpret_cast<const char*>(data), length);
            }
        });

        topic_to_msg_id_.clear();

        writer_->fileHeader(ulog_cpp::FileHeader(current_timestamp_us()));

        if (!write_known_topics()) {
            close();
            return false;
        }

        writer_->headerComplete();

        for (const auto& [topic_name, msg_id] : topic_to_msg_id_) {
            writer_->addLoggedMessage(ulog_cpp::AddLoggedMessage(0, msg_id, topic_name.toStdString()));
        }
        return true;
    }

    bool is_open() const { return file_.isOpen(); }

    void close()
    {
        if (file_.isOpen()) {
            file_.flush();
            file_.close();
        }
        writer_.reset();
        topic_to_msg_id_.clear();
    }

    bool write_record(const log_manager::packet_record& rec)
    {
        if (!writer_) {
            return false;
        }

        const QString topic_name = rec.topic_name.trimmed();
        if (topic_name.isEmpty()) {
            return true;
        }

        const auto it = topic_to_msg_id_.find(topic_name);
        if (it == topic_to_msg_id_.end()) {
            return true;
        }

        const mavlink_message_info_t* info = mavlink_get_message_info(&rec.message);
        if (info == nullptr) {
            return true;
        }

        const std::vector<uint8_t> payload = build_payload(rec, info);
        if (payload.empty()) {
            return false;
        }

        writer_->data(ulog_cpp::Data(it->second, payload));
        return true;
    }

private:
    static uint64_t current_timestamp_us()
    {
        return static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()) * 1000ULL;
    }

    bool write_known_topics()
    {
        for (quint32 msg_id = 0; msg_id <= std::numeric_limits<quint16>::max(); ++msg_id) {
            const mavlink_message_info_t* info = mavlink_get_message_info_by_id(static_cast<quint16>(msg_id));
            if (!info || info->name == nullptr || info->name[0] == '\0') {
                continue;
            }

            const QString topic_name = QString::fromLatin1(info->name);
            if (topic_to_msg_id_.contains(topic_name)) {
                continue;
            }

            const quint16 assigned = static_cast<quint16>(topic_to_msg_id_.size());
            topic_to_msg_id_.emplace(topic_name, assigned);

            if (!write_topic_format(topic_name, info)) {
                return false;
            }
        }

        return true;
    }

    bool write_topic_format(const QString& topic_name, const mavlink_message_info_t* info)
    {
        const std::vector<ulog_cpp::Field> fields = build_message_fields(info);
        if (fields.empty()) {
            return false;
        }
        writer_->messageFormat(ulog_cpp::MessageFormat(topic_name.toStdString(), fields));
        return true;
    }

    static std::vector<ulog_cpp::Field> build_message_fields(const mavlink_message_info_t* info)
    {
        std::vector<ulog_cpp::Field> fields;
        if (info == nullptr) {
            return fields;
        }

        const mavlink_field_info_t* timestamp_field = nullptr;
        for (unsigned int i = 0; i < info->num_fields; ++i) {
            const mavlink_field_info_t& field = info->fields[i];
            if (field.name != nullptr && std::strcmp(field.name, "timestamp") == 0) {
                timestamp_field = &field;
                break;
            }
        }

        if (timestamp_field != nullptr) {
            const char* type = ulog_type_for_mav_type(timestamp_field->type);
            if (type == nullptr) {
                return {};
            }
            fields.emplace_back(type, "timestamp", timestamp_field->array_length > 0 ? static_cast<int>(timestamp_field->array_length) : -1);
        } else {
            fields.emplace_back("uint64_t", "timestamp");
        }

        for (unsigned int i = 0; i < info->num_fields; ++i) {
            const mavlink_field_info_t& field = info->fields[i];
            if (field.name == nullptr || field.name[0] == '\0' || std::strcmp(field.name, "timestamp") == 0) {
                continue;
            }

            const char* type = ulog_type_for_mav_type(field.type);
            if (type == nullptr) {
                return {};
            }

            fields.emplace_back(type,
                                field.name,
                                field.array_length > 0 ? static_cast<int>(field.array_length) : -1);
        }

        return fields;
    }

    static const mavlink_field_info_t* timestamp_field(const mavlink_message_info_t* info)
    {
        if (info == nullptr) {
            return nullptr;
        }

        for (unsigned int i = 0; i < info->num_fields; ++i) {
            const mavlink_field_info_t& field = info->fields[i];
            if (field.name != nullptr && std::strcmp(field.name, "timestamp") == 0) {
                return &field;
            }
        }

        return nullptr;
    }

    std::vector<uint8_t> build_payload(const log_manager::packet_record& rec, const mavlink_message_info_t* info) const
    {
        QByteArray payload;
        QDataStream ds(&payload, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::LittleEndian);

        const mavlink_field_info_t* ts_field = timestamp_field(info);
        if (ts_field != nullptr) {
            if (!append_mavlink_field_value(ds, rec.message, *ts_field)) {
                return {};
            }
        } else {
            ds << static_cast<quint64>(rec.timestamp_us);
        }

        for (unsigned int i = 0; i < info->num_fields; ++i) {
            const mavlink_field_info_t& field = info->fields[i];
            if (field.name == nullptr || field.name[0] == '\0' || std::strcmp(field.name, "timestamp") == 0) {
                continue;
            }

            if (!append_mavlink_field_value(ds, rec.message, field)) {
                return {};
            }
        }

        return std::vector<uint8_t>(payload.begin(), payload.end());
    }

    QFile file_;
    std::map<QString, quint16> topic_to_msg_id_;
    std::unique_ptr<ulog_cpp::Writer> writer_;
};

QString build_unique_session_name(const QString& base_directory)
{
    QDir dir(base_directory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    const QString stamp = QDateTime::currentDateTime().toString("yyyy_MM_dd_HH_mm_ss_zzz");
    QString candidate = dir.filePath(stamp);

    int suffix = 1;
    while (QFileInfo::exists(candidate)) {
        candidate = dir.filePath(stamp + "_" + QString::number(suffix));
        ++suffix;
    }
    return QFileInfo(candidate).fileName();
}

QString build_stream_file_name(const QString& port_name, uint8_t direction_value, const QString& session_name)
{
    const QString sanitized_port = sanitize_topic_component(port_name);
    const QString dir = direction_label(direction_value);
    return QStringLiteral("%1_%2_%3.ulg").arg(sanitized_port, dir, session_name);
}

} // namespace

log_manager& log_manager::instance()
{
    static log_manager inst;
    return inst;
}

QString log_manager::default_log_directory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    return QDir(base).filePath("KGroundControl/logs");
}

log_manager::log_manager()
{
    directory_ = default_log_directory();
    writer_thread_ = std::thread(&log_manager::writer_loop, this);
}

log_manager::~log_manager()
{
    shutdown();
}

void log_manager::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (stop_requested_) return;
        stop_requested_ = true;
        config_dirty_ = true;
    }
    enabled_fast_.store(false, std::memory_order_release);
    state_cv_.notify_one();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
}

void log_manager::apply_settings(const kgroundcontrol_settings& settings)
{
    const QString new_directory = settings.mavlink_logging_directory.trimmed().isEmpty()
        ? default_log_directory()
        : settings.mavlink_logging_directory.trimmed();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        const bool enabled_changed = (enabled_config_ != settings.mavlink_logging_enabled);
        const bool directory_changed = (directory_ != new_directory);

        enabled_config_ = settings.mavlink_logging_enabled;
        directory_ = new_directory;
        config_dirty_ = true;

        if (enabled_config_ && (enabled_changed || directory_changed)) {
            rotate_requested_ = true;
        }
        if (!enabled_config_) {
            rotate_requested_ = false;
        }
    }

    enabled_fast_.store(settings.mavlink_logging_enabled, std::memory_order_release);
    state_cv_.notify_one();
}

log_manager::packet_record log_manager::make_record(io_direction dir, const QString& port_name, const mavlink_message_t& message)
{
    packet_record rec;
    rec.timestamp_us = static_cast<uint64_t>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()) * 1000ULL;
    rec.message = message;
    rec.direction_value = static_cast<uint8_t>(dir);
    rec.port_name = port_name;

    if (const mavlink_message_info_t* info = mavlink_get_message_info(&rec.message)) {
        if (info->name != nullptr) {
            rec.topic_name = QString::fromLatin1(info->name);
        }
    }

    return rec;
}

void log_manager::enqueue_record(packet_record&& record)
{
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        queue_.push_back(std::move(record));
    }
    state_cv_.notify_one();
}

void log_manager::log_incoming_message(const QString& port_name, const mavlink_message_t& message)
{
    if (!enabled_fast_.load(std::memory_order_acquire)) return;
    enqueue_record(make_record(io_direction::incoming, port_name, message));
}

void log_manager::log_outgoing_message(const QString& port_name, const mavlink_message_t& message)
{
    if (!enabled_fast_.load(std::memory_order_acquire)) return;
    enqueue_record(make_record(io_direction::outgoing, port_name, message));
}

void log_manager::log_outgoing_bytes(const QString& port_name, const QByteArray& bytes)
{
    if (!enabled_fast_.load(std::memory_order_acquire)) return;
    if (bytes.isEmpty()) return;

    QVector<mavlink_message_t> parsed_messages;
    {
        QMutexLocker locker(&outgoing_parse_mutex_);
        mavlink_status_t& parse_status = outgoing_parse_status_[port_name];
        mavlink_message_t parsed{};

        for (const char b : bytes) {
            const bool got = static_cast<bool>(mavlink_parse_char(MAVLINK_COMM_0,
                                                                  static_cast<uint8_t>(b),
                                                                  &parsed,
                                                                  &parse_status));
            if (got) {
                parsed_messages.push_back(parsed);
            }
        }
    }

    for (const auto& msg : parsed_messages) {
        log_outgoing_message(port_name, msg);
    }
}

void log_manager::writer_loop()
{
    std::map<QString, std::unique_ptr<ulog_stream_writer>> writers;
    QString session_name;
    QString session_dir;

    auto close_all_writers = [&writers]() {
        for (auto it = writers.begin(); it != writers.end(); ++it) {
            if (it->second) {
                it->second->close();
            }
        }
        writers.clear();
    };

    auto write_record = [&writers, &session_name, &session_dir](const packet_record& rec) -> bool {
        const QString stream_key = rec.port_name + QLatin1Char('|') + direction_label(rec.direction_value);
        auto it = writers.find(stream_key);
        if (it == writers.end()) {
            std::unique_ptr<ulog_stream_writer> new_writer = std::make_unique<ulog_stream_writer>();
            const QString stream_file_name = build_stream_file_name(rec.port_name, rec.direction_value, session_name);
            const QString stream_file_path = QDir(session_dir).filePath(stream_file_name);
            if (!new_writer->open(stream_file_path)) {
                return false;
            }
            auto inserted = writers.emplace(stream_key, std::move(new_writer));
            it = inserted.first;
        }

        ulog_stream_writer* writer = it->second.get();
        if (!writer || !writer->is_open()) {
            return false;
        }

        return writer->write_record(rec);
    };

    while (true) {
        std::deque<packet_record> local_queue;
        bool local_stop = false;
        bool local_enabled = false;
        bool local_rotate = false;
        QString local_directory;

        std::unique_lock<std::mutex> lock(state_mutex_);
        state_cv_.wait(lock, [this]() {
            return stop_requested_ || !queue_.empty() || config_dirty_;
        });

        local_stop = stop_requested_;
        local_enabled = enabled_config_;
        local_rotate = rotate_requested_;
        local_directory = directory_;

        rotate_requested_ = false;
        config_dirty_ = false;
        queue_.swap(local_queue);

        lock.unlock();
        if (local_stop && local_queue.empty()) {
            break;
        }

        if (!local_enabled) {
            if (!writers.empty()) {
                close_all_writers();
                session_name.clear();
                session_dir.clear();
            }
            continue;
        }

        if (local_rotate || session_dir.isEmpty()) {
            close_all_writers();
            session_name = build_unique_session_name(local_directory);
            session_dir = QDir(local_directory).filePath(session_name);
            if (!QDir().mkpath(session_dir)) {
                session_name.clear();
                session_dir.clear();
                continue;
            }
        }

        for (const auto& rec : local_queue) {
            if (!write_record(rec)) {
                close_all_writers();
                session_name.clear();
                session_dir.clear();
                break;
            }
        }
    }

    close_all_writers();
}

