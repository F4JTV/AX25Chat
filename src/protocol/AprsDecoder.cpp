/*
 * AprsDecoder.cpp - decoding received APRS packets.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AprsDecoder.h"
#include "AX25Frame.h"

#include <QRegularExpression>
#include <QtMath>

#include <cmath>
#include <cstring>

#include "DirewolfHeaders.h"

extern "C" {
#include "dw_embed.h"
}

namespace {

constexpr double MphToKmh = 1.609344;
constexpr double InchToMm = 25.4;
constexpr double MphToKnots = 0.868976;
constexpr double FeetToMetres = 0.3048;
constexpr double MilesToKm = 1.609344;

// Data type identifiers, APRS Protocol Reference 1.0.1 chapter 5.
constexpr char DtiPositionNoTs = '!';
constexpr char DtiPositionNoTsMsg = '=';
constexpr char DtiPositionTs = '/';
constexpr char DtiPositionTsMsg = '@';
constexpr char DtiMessage = ':';
constexpr char DtiStatus = '>';
constexpr char DtiObject = ';';
constexpr char DtiItem = ')';
constexpr char DtiMicEOld = '\'';
constexpr char DtiMicE = '`';
constexpr char DtiWeather = '_';
constexpr char DtiTelemetry = 'T';
constexpr char DtiQuery = '?';
constexpr char DtiCapabilities = '<';
constexpr char DtiThirdParty = '}';

bool known(double value) { return value != G_UNKNOWN; }

std::optional<double> optDouble(double value)
{
    return known(value) ? std::optional<double>(value) : std::nullopt;
}

QString fromC(const char *text)
{
    return text ? QString::fromLocal8Bit(text) : QString();
}

// Messages decode_aprs() prints about a packet, collected per call.
struct Capture
{
    QStringList errors;
};

void captureLine(void *user, int level, const char *line)
{
    auto *capture = static_cast<Capture *>(user);
    if (level != 1 /* DW_COLOR_ERROR */) return;
    const QString text = fromC(line).trimmed();
    if (text.isEmpty()) return;
    // Advice about the destination address and a missing tocalls.yaml are
    // not problems with the packet.
    if (text.contains(QStringLiteral("obsolete")) || text.startsWith(QStringLiteral("Tell the sender"))
        || text.startsWith(QStringLiteral("deviceid_decode_dest called without"))) return;
    capture->errors.append(text);
}


// ---- Weather ------------------------------------------------------------------

// Pull one field out of the text.  A field whose digits are dots or spaces
// means "not measured" and yields nullopt with found = true.
std::optional<double> takeField(QString &text, QChar code, int digits, bool *found)
{
    if (found) *found = false;
    int index = text.indexOf(code);
    while (index >= 0) {
        const QString chunk = text.mid(index + 1, digits);
        if (chunk.size() == digits) {
            bool blank = true;
            for (const QChar c : chunk) {
                if (c != '.' && c != ' ') { blank = false; break; }
            }
            if (blank) {
                text.remove(index, 1 + digits);
                if (found) *found = true;
                return std::nullopt;
            }
            bool ok = false;
            const double value = chunk.toDouble(&ok);
            if (ok) {
                text.remove(index, 1 + digits);
                if (found) *found = true;
                return value;
            }
        }
        index = text.indexOf(code, index + 1);
    }
    return std::nullopt;
}

} // namespace


bool AprsWeather::any() const
{
    return windDirectionDeg || windSpeedKmh || windGustKmh || temperatureC || rainHourMm
        || rain24hMm || rainMidnightMm || humidityPct || pressureHpa || luminosityWm2
        || snow24hCm || radiation;
}

QString AprsWeather::describe() const
{
    QStringList parts;
    if (temperatureC) parts << QStringLiteral("%1 C").arg(*temperatureC, 0, 'f', 1);
    if (humidityPct) parts << QStringLiteral("%1% RH").arg(*humidityPct);
    if (windSpeedKmh || windGustKmh) {
        QString wind = windSpeedKmh ? QStringLiteral("wind %1 km/h").arg(qRound(*windSpeedKmh))
                                    : QStringLiteral("wind");
        if (windDirectionDeg) {
            wind += QStringLiteral(" from %1 %2")
                        .arg(*windDirectionDeg, 3, 10, QChar('0'))
                        .arg(AprsDecoder::compassPoint(*windDirectionDeg));
        }
        if (windGustKmh && *windGustKmh != 0.0) {
            wind += QStringLiteral(", gusting %1").arg(qRound(*windGustKmh));
        }
        parts << wind;
    }
    if (pressureHpa) parts << QStringLiteral("%1 hPa").arg(*pressureHpa, 0, 'f', 1);
    if (rainHourMm && *rainHourMm != 0.0) parts << QStringLiteral("rain %1 mm/h").arg(*rainHourMm, 0, 'f', 1);
    if (rain24hMm && *rain24hMm != 0.0) parts << QStringLiteral("%1 mm/24h").arg(*rain24hMm, 0, 'f', 1);
    if (snow24hCm && *snow24hCm != 0.0) parts << QStringLiteral("snow %1 cm/24h").arg(*snow24hCm, 0, 'f', 1);
    if (luminosityWm2) parts << QStringLiteral("%1 W/m2").arg(*luminosityWm2);
    if (radiation) parts << QStringLiteral("radiation %1").arg(qRound(*radiation));
    return parts.isEmpty() ? QStringLiteral("no readings") : parts.join(QStringLiteral(", "));
}

std::optional<AprsWeather> AprsWeather::parse(const QString &text, QString *rest)
{
    AprsWeather data;
    QString remaining = text;

    if (auto v = takeField(remaining, 'c', 3, nullptr)) data.windDirectionDeg = static_cast<int>(*v) % 360;
    if (auto v = takeField(remaining, 's', 3, nullptr)) data.windSpeedKmh = *v * MphToKmh;
    if (auto v = takeField(remaining, 'g', 3, nullptr)) data.windGustKmh = *v * MphToKmh;

    // Temperature may be negative, written as t-05, so it is handled apart
    // from the plain digit fields.
    static const QRegularExpression tempRe(QStringLiteral("t(-?\\d{2,3})"));
    const auto temp = tempRe.match(remaining);
    if (temp.hasMatch()) {
        const double fahrenheit = temp.captured(1).toDouble();
        data.temperatureC = (fahrenheit - 32.0) / 1.8;
        remaining.remove(temp.capturedStart(), temp.capturedLength());
    }

    if (auto v = takeField(remaining, 'r', 3, nullptr)) data.rainHourMm = *v / 100.0 * InchToMm;
    if (auto v = takeField(remaining, 'p', 3, nullptr)) data.rain24hMm = *v / 100.0 * InchToMm;
    if (auto v = takeField(remaining, 'P', 3, nullptr)) data.rainMidnightMm = *v / 100.0 * InchToMm;
    if (auto v = takeField(remaining, 'b', 5, nullptr)) data.pressureHpa = *v / 10.0;
    if (auto v = takeField(remaining, 'L', 3, nullptr)) data.luminosityWm2 = static_cast<int>(*v);
    if (auto v = takeField(remaining, 'l', 3, nullptr)) data.luminosityWm2 = static_cast<int>(*v) + 1000;
    if (auto v = takeField(remaining, 'S', 3, nullptr)) data.snow24hCm = *v * 2.54;
    if (auto v = takeField(remaining, 'X', 3, nullptr)) data.radiation = *v;

    if (auto v = takeField(remaining, 'h', 2, nullptr)) {
        // Humidity 00 means 100 percent; there is no other way to write
        // three digits in a two digit field.
        const int h = static_cast<int>(*v);
        data.humidityPct = (h == 0) ? 100 : h;
    }

    if (rest) *rest = remaining.trimmed();
    if (!data.any()) return std::nullopt;
    return data;
}


// ---- AprsPacket names ---------------------------------------------------------

QString AprsPacket::kindName(Kind kind)
{
    switch (kind) {
        case Kind::Position: return QStringLiteral("position");
        case Kind::Message:  return QStringLiteral("message");
        case Kind::Status:   return QStringLiteral("status");
        case Kind::Object:   return QStringLiteral("object");
        case Kind::Item:     return QStringLiteral("item");
        case Kind::Weather:  return QStringLiteral("weather");
        case Kind::Other:    return QStringLiteral("other");
        default:             return QStringLiteral("unknown");
    }
}

QString AprsPacket::messageKindName(MessageKind kind)
{
    switch (kind) {
        case MessageKind::Message:  return QStringLiteral("message");
        case MessageKind::Ack:      return QStringLiteral("ack");
        case MessageKind::Rej:      return QStringLiteral("rej");
        case MessageKind::Bulletin: return QStringLiteral("bulletin");
        case MessageKind::Invalid:  return QStringLiteral("invalid");
        default:                    return QString();
    }
}


// ---- Decoder --------------------------------------------------------------------

QString AprsDecoder::describeDataType(QChar dti)
{
    switch (dti.toLatin1()) {
        case DtiPositionNoTs:    return QStringLiteral("Position");
        case DtiPositionNoTsMsg: return QStringLiteral("Position, messaging");
        case DtiPositionTs:      return QStringLiteral("Position with timestamp");
        case DtiPositionTsMsg:   return QStringLiteral("Position with timestamp, messaging");
        case DtiMessage:         return QStringLiteral("Message");
        case DtiStatus:          return QStringLiteral("Status");
        case DtiObject:          return QStringLiteral("Object");
        case DtiItem:            return QStringLiteral("Item");
        case DtiMicE:            return QStringLiteral("Mic-E");
        case DtiMicEOld:         return QStringLiteral("Mic-E, old");
        case DtiWeather:         return QStringLiteral("Weather report");
        case DtiTelemetry:       return QStringLiteral("Telemetry");
        case DtiQuery:           return QStringLiteral("Query");
        case DtiCapabilities:    return QStringLiteral("Station capabilities");
        case DtiThirdParty:      return QStringLiteral("Third-party traffic");
        default:                 return QStringLiteral("Unknown type");
    }
}

double AprsDecoder::bearingDeg(double lat1, double lon1, double lat2, double lon2)
{
    const double phi1 = qDegreesToRadians(lat1);
    const double phi2 = qDegreesToRadians(lat2);
    const double dlambda = qDegreesToRadians(lon2 - lon1);
    const double y = std::sin(dlambda) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(dlambda);
    return std::fmod(qRadiansToDegrees(std::atan2(y, x)) + 360.0, 360.0);
}

double AprsDecoder::distanceKm(double lat1, double lon1, double lat2, double lon2)
{
    const double radius = 6371.0;
    const double phi1 = qDegreesToRadians(lat1);
    const double phi2 = qDegreesToRadians(lat2);
    const double dphi = qDegreesToRadians(lat2 - lat1);
    const double dlambda = qDegreesToRadians(lon2 - lon1);
    const double a = std::sin(dphi / 2) * std::sin(dphi / 2)
                   + std::cos(phi1) * std::cos(phi2) * std::sin(dlambda / 2) * std::sin(dlambda / 2);
    return 2 * radius * std::asin(std::min(1.0, std::sqrt(a)));
}

QString AprsDecoder::compassPoint(double bearing)
{
    static const char *points[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                   "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
    const int index = static_cast<int>(std::fmod(bearing + 11.25, 360.0) / 22.5);
    return QString::fromLatin1(points[qBound(0, index, 15)]);
}


namespace {

// ---- Messages: addressee, ack/rej, identifier, reply-ack, bulletins ------------

void decodeMessage(AprsPacket &packet, const QString &body)
{
    // Message identifier at the end of the text.  Three forms exist:
    //   {12       plain identifier, APRS 1.0
    //   {12}      identifier, and the sender understands reply-ack
    //   {12}34    identifier, plus an acknowledgement of message 34
    static const QRegularExpression msgIdRe(QStringLiteral("\\{([A-Za-z0-9]{1,5})(\\}([A-Za-z0-9]{1,2})?)?\\s*$"));
    // An acknowledgement may also carry a trailing brace, e.g. "ack12}".
    static const QRegularExpression ackRe(QStringLiteral("^(ack|rej)([A-Za-z0-9]{1,5})\\}?\\s*$"),
                                          QRegularExpression::CaseInsensitiveOption);

    packet.kind = AprsPacket::Kind::Message;

    // ":" + 9 character addressee + ":" + text
    if (body.size() < 10 || body.at(9) != ':') {
        packet.errors.append(QStringLiteral("A message needs a 9 character addressee between two colons"));
        packet.messageKind = AprsPacket::MessageKind::Invalid;
        return;
    }
    packet.addressee = body.left(9).trimmed();
    QString text = body.mid(10);
    while (text.endsWith('\r') || text.endsWith('\n')) text.chop(1);

    const auto ack = ackRe.match(text);
    if (ack.hasMatch()) {
        packet.messageKind = ack.captured(1).compare(QStringLiteral("ack"), Qt::CaseInsensitive) == 0
                                 ? AprsPacket::MessageKind::Ack : AprsPacket::MessageKind::Rej;
        packet.messageId = ack.captured(2);
        packet.text.clear();
        return;
    }

    const auto identifier = msgIdRe.match(text);
    if (identifier.hasMatch()) {
        packet.messageId = identifier.captured(1);
        if (identifier.capturedStart(2) >= 0) {
            packet.replyAckCapable = true;
            packet.ackOf = identifier.captured(3);
        }
        text = text.left(identifier.capturedStart(0));
    }
    packet.text = text;

    // BLN is the general bulletin prefix; NWS, SKY, CWA and BOM are the
    // weather services' own, listed in Direwolf's aprs_message().  All are
    // broadcasts and none is ever acknowledged.
    const QString prefix = packet.addressee.toUpper().left(3);
    static const QStringList bulletinPrefixes = {QStringLiteral("BLN"), QStringLiteral("NWS"), QStringLiteral("SKY"),
                                                 QStringLiteral("CWA"), QStringLiteral("BOM")};
    packet.messageKind = bulletinPrefixes.contains(prefix) ? AprsPacket::MessageKind::Bulletin
                                                           : AprsPacket::MessageKind::Message;
}


// ---- Weather: locate the readings in the raw text -------------------------------

// Length of the position block that starts at body[0]: an uncompressed
// position starts with the latitude degrees, always digits (or spaces for
// ambiguity); a compressed one starts with the symbol table, never a digit.
int positionBlockLength(const QString &body)
{
    if (body.isEmpty()) return 0;
    const QChar c = body.at(0);
    return (c.isDigit() || c == ' ') ? 19 : 13;
}

// The text after the position of a packet, where a weather station keeps
// its readings.  Empty when the packet has no position we can measure.
QString textAfterPosition(QChar dti, const QString &body)
{
    QString rest;
    switch (dti.toLatin1()) {
        case DtiPositionNoTs:
        case DtiPositionNoTsMsg:
            rest = body;
            break;
        case DtiPositionTs:
        case DtiPositionTsMsg:
            rest = body.mid(7);
            break;
        case DtiObject:
            rest = body.mid(9 + 1 + 7);
            break;
        case DtiItem: {
            int end = -1;
            for (int i = 0; i < qMin(9, body.size()); i++) {
                if (body.at(i) == '!' || body.at(i) == '_') { end = i; break; }
            }
            if (end < 0) return QString();
            rest = body.mid(end + 1);
            break;
        }
        default:
            return QString();
    }
    return rest.mid(positionBlockLength(rest));
}

// Readings of a weather station position: Direwolf's rule that a leading
// nnn/nnn is wind direction and speed in mph, not course and speed, then
// the field codes.  Returns what is left, the station's own comment.
std::optional<AprsWeather> weatherFromPosition(const QString &afterPosition, QString *comment)
{
    static const QRegularExpression cseSpdRe(QStringLiteral("^(\\d{3})/(\\d{3})"));

    QString text = afterPosition;
    AprsWeather wind;
    bool haveWind = false;

    const auto m = cseSpdRe.match(text);
    if (m.hasMatch()) {
        wind.windDirectionDeg = m.captured(1).toInt() % 360;
        wind.windSpeedKmh = m.captured(2).toDouble() * MphToKmh;
        haveWind = true;
        text = text.mid(7);
    }

    QString rest;
    auto fields = AprsWeather::parse(text, &rest);
    if (!fields && !haveWind) {
        if (comment) *comment = text.trimmed();
        return std::nullopt;
    }
    AprsWeather result = fields.value_or(AprsWeather());
    if (haveWind) {
        if (!result.windDirectionDeg) result.windDirectionDeg = wind.windDirectionDeg;
        if (!result.windSpeedKmh) result.windSpeedKmh = wind.windSpeedKmh;
    }
    if (comment) *comment = rest;
    return result;
}


// ---- Direwolf ---------------------------------------------------------------------

void takeDirewolfFields(AprsPacket &packet, const decode_aprs_t &A)
{
    packet.detail = fromC(A.g_data_type_desc);

    if (known(A.g_lat) && known(A.g_lon)) {
        packet.latitude = A.g_lat;
        packet.longitude = A.g_lon;
    }
    if (A.g_symbol_table != 0) packet.symbolTable = QChar(static_cast<unsigned char>(A.g_symbol_table));
    if (A.g_symbol_code != 0) packet.symbolCode = QChar(static_cast<unsigned char>(A.g_symbol_code));

    if (known(A.g_course)) packet.courseDeg = A.g_course;
    if (known(A.g_speed_mph)) packet.speedKnots = A.g_speed_mph * MphToKnots;
    if (known(A.g_altitude_ft)) packet.altitudeM = A.g_altitude_ft * FeetToMetres;

    packet.comment = fromC(A.g_comment).trimmed();
    packet.name = fromC(A.g_name).trimmed();
    packet.maidenhead = fromC(A.g_maidenhead);
    packet.manufacturer = fromC(A.g_mfr);
    if (packet.manufacturer.contains(QStringLiteral("UNKNOWN vendor/model"))) packet.manufacturer.clear();
    if (known(A.g_freq) && A.g_freq != 0.0) packet.frequencyMHz = A.g_freq;
    if (known(A.g_range) && A.g_range != 0.0) packet.rangeKm = A.g_range * MilesToKm;
    packet.thirdParty = A.g_has_thirdparty_header != 0;

    const QString micE = fromC(A.g_mic_e_status);
    if (!micE.isEmpty()) packet.status = micE;
}

} // namespace


AprsPacket AprsDecoder::decode(const QByteArray &info, const QString &destination, const QString &source)
{
    AprsPacket packet;
    if (info.isEmpty()) {
        packet.errors.append(QStringLiteral("Empty information field"));
        return packet;
    }

    const auto first = static_cast<unsigned char>(info.at(0));
    const QChar dti = first < 128 ? QChar(first) : QChar('?');
    packet.dataType = dti;
    packet.description = describeDataType(dti);

    // The parts done here.
    QString body = QString::fromUtf8(info.mid(1));
    while (body.endsWith('\r') || body.endsWith('\n')) body.chop(1);

    if (dti == DtiMessage) {
        decodeMessage(packet, body);
        return packet;
    }

    if (dti == DtiWeather) {
        packet.kind = AprsPacket::Kind::Weather;
        // Positionless report: _MMDDHHMM then the readings.
        QString readings = body;
        if (readings.size() >= 8) {
            bool allDigits = true;
            for (int i = 0; i < 8; i++) if (!readings.at(i).isDigit()) { allDigits = false; break; }
            if (allDigits) {
                packet.timestamp = readings.left(8);
                readings = readings.mid(8);
            }
        }
        QString comment;
        packet.weather = AprsWeather::parse(readings, &comment);
        packet.comment = comment;
        return packet;
    }

    if (dti == DtiTelemetry || dti == DtiQuery || dti == DtiCapabilities) {
        // Recognised but not decoded; the type name is enough for the
        // monitor to label it instead of showing raw bytes.
        packet.kind = AprsPacket::Kind::Other;
        packet.comment = body.trimmed();
        return packet;
    }

    // Everything with a position, plus status and third-party traffic,
    // goes through Direwolf.  A packet_t is built from a UI frame with the
    // real destination, which Mic-E needs.
    // The frame handed to the decoder must carry AX.25 addresses: at most six
    // characters and a numeric SSID from 0 to 15.  Packets that never went
    // over the air do not have to obey that: on APRS-IS a station may well
    // be F4ABC-D (a DMR hotspot), N0CALL-23 or IR1ZWA-S.  The decoder only
    // reads the information field and, for Mic-E, the destination; an
    // address it would reject is replaced by a placeholder for the
    // decoder's benefit, and the real one is kept by the caller.
    auto usable = [](const QString &address) {
        QString call;
        int ssid = 0;
        return !address.isEmpty() && ax25::splitCallsign(address, call, ssid);
    };
    char addrs[AX25_MAX_ADDRS][AX25_MAX_ADDR_LEN];
    std::memset(addrs, 0, sizeof(addrs));
    const QByteArray dest = usable(destination) ? destination.toLatin1().left(9) : QByteArray("APRS");
    const QByteArray src = usable(source) ? source.toLatin1().left(9) : QByteArray("NOCALL");
    std::strncpy(addrs[AX25_DESTINATION], dest.constData(), AX25_MAX_ADDR_LEN - 1);
    std::strncpy(addrs[AX25_SOURCE], src.constData(), AX25_MAX_ADDR_LEN - 1);
    QByteArray infoCopy = info;
    // Everything Direwolf prints while building and decoding is collected
    // here, never shown as if the modem had said it.
    Capture capture;
    dw_textcolor_capture_begin(captureLine, &capture);
    packet_t pp = dw::uiFrame(addrs, 2, 0xF0, reinterpret_cast<unsigned char *>(infoCopy.data()),
                              infoCopy.size());
    if (pp == nullptr) {
        dw_textcolor_capture_end();
        packet.errors.append(QStringLiteral("Could not build a packet for the decoder (bad address?)"));
        packet.errors.append(capture.errors);
        if (packet.kind == AprsPacket::Kind::Unknown) packet.comment = body.trimmed();
        return packet;
    }
    dw_embed_init_tables();
    decode_aprs_t A;
    std::memset(&A, 0, sizeof(A));
    decode_aprs(&A, pp, 0, nullptr);
    dw_textcolor_capture_end();
    dw::del(pp);
    packet.errors.append(capture.errors);

    takeDirewolfFields(packet, A);

    switch (dti.toLatin1()) {
        case DtiPositionNoTs:
        case DtiPositionNoTsMsg:
        case DtiPositionTs:
        case DtiPositionTsMsg:
        case DtiMicE:
        case DtiMicEOld:
            packet.kind = AprsPacket::Kind::Position;
            if (dti == DtiPositionTs || dti == DtiPositionTsMsg) packet.timestamp = body.left(7);
            break;
        case DtiObject:
            packet.kind = AprsPacket::Kind::Object;
            packet.alive = body.size() > 9 && body.at(9) == '*';
            if (body.size() >= 17) packet.timestamp = body.mid(10, 7);
            break;
        case DtiItem:
            packet.kind = AprsPacket::Kind::Item;
            for (int i = 0; i < qMin(9, body.size()); i++) {
                if (body.at(i) == '!' || body.at(i) == '_') { packet.alive = body.at(i) == '!'; break; }
            }
            break;
        case DtiStatus:
            packet.kind = AprsPacket::Kind::Status;
            packet.status = body.trimmed();
            break;
        case DtiThirdParty:
            switch (A.g_packet_type) {
                case decode_aprs_t::packet_type_position:  packet.kind = AprsPacket::Kind::Position; break;
                case decode_aprs_t::packet_type_weather:   packet.kind = AprsPacket::Kind::Weather; break;
                case decode_aprs_t::packet_type_object:    packet.kind = AprsPacket::Kind::Object; break;
                case decode_aprs_t::packet_type_item:      packet.kind = AprsPacket::Kind::Item; break;
                case decode_aprs_t::packet_type_message:   packet.kind = AprsPacket::Kind::Message; break;
                case decode_aprs_t::packet_type_status:    packet.kind = AprsPacket::Kind::Status; break;
                case decode_aprs_t::packet_type_none:      packet.kind = AprsPacket::Kind::Unknown; break;
                default:                                   packet.kind = AprsPacket::Kind::Other; break;
            }
            if (packet.kind == AprsPacket::Kind::Message) {
                packet.addressee = fromC(A.g_addressee).trimmed();
                packet.text = packet.comment;
                packet.messageId = fromC(A.g_message_number);
                packet.messageKind = AprsPacket::MessageKind::Message;
            }
            break;
        default:
            packet.kind = AprsPacket::Kind::Unknown;
            packet.comment = body.trimmed();
            break;
    }

    if (!packet.hasPosition() && (packet.kind == AprsPacket::Kind::Position
                                  || packet.kind == AprsPacket::Kind::Object
                                  || packet.kind == AprsPacket::Kind::Item)) {
        if (packet.errors.isEmpty()) packet.errors.append(QStringLiteral("Malformed coordinates"));
    }

    // A position whose symbol code is '_' is a weather station; its readings
    // are appended to the comment rather than sent separately.
    if (packet.hasPosition() && packet.symbolCode == '_') {
        const QString after = textAfterPosition(dti, body);
        if (!after.isEmpty()) {
            QString comment;
            packet.weather = weatherFromPosition(after, &comment);
            if (packet.weather) {
                packet.comment = comment;
                // Direwolf may have read the wind prefix as course/speed.
                packet.courseDeg.reset();
                packet.speedKnots.reset();
            }
        }
    }

    return packet;
}

AprsPacket AprsDecoder::decode(const AX25Frame &frame)
{
    if (!frame.isUi() || !frame.pid || *frame.pid != ax25::PidNoLayer3) {
        AprsPacket packet;
        packet.errors.append(QStringLiteral("Not an APRS frame (UI, PID F0)"));
        return packet;
    }
    return decode(frame.info, frame.destination.toString(), frame.source.toString());
}
