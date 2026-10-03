/*
 * AprsDecoder.h - decoding received APRS packets.
 *
 * The heavy lifting is Direwolf's decode_aprs.c, compiled into
 * direwolf_core: uncompressed and base-91 compressed positions, positions
 * with a timestamp, !DAO! precision, objects, items, status reports, Mic-E
 * (whose latitude lives in the AX.25 destination address), capabilities,
 * queries, telemetry, third-party headers.
 *
 * Two things are done here rather than there, to keep the semantics of the
 * Python decoder the rest of the application was written against:
 *
 *   - messages: addressee, ack/rej, message identifier, the reply-ack forms
 *     {12} and {12}34 of the APRS 1.2 addendum, and bulletin detection.
 *     Direwolf reports the piggybacked acknowledgement only in its printed
 *     description, and the messaging layer needs it as a field.
 *   - weather: Direwolf renders readings into a text line in imperial
 *     units; the station list wants numbers in metric, so the field codes
 *     are decoded again from the raw text, with Direwolf's own rule that a
 *     leading nnn/nnn on a weather station is wind, not course and speed.
 *
 * Never throws and never fails: an undecodable packet comes back with kind
 * Unknown and a note in errors, because a malformed packet from the air must
 * not be able to stop the receive path.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AprsPacket.h"

#include <QByteArray>
#include <QString>

class AX25Frame;

class AprsDecoder
{
public:
    // Decode the information field of a frame.  The destination is needed
    // for Mic-E and for the manufacturer lookup; the source only appears in
    // Direwolf's descriptions.
    static AprsPacket decode(const QByteArray &info, const QString &destination,
                             const QString &source = QStringLiteral("NOCALL"));

    // Same, from a decoded frame.  Non-UI frames and frames with a PID
    // other than 0xF0 are not APRS and come back as Unknown.
    static AprsPacket decode(const AX25Frame &frame);

    // Short type name for a data type identifier, e.g. '!' -> "Position".
    static QString describeDataType(QChar dti);

    // Initial bearing from one point to another, degrees clockwise from
    // north, and the compass point for a bearing.
    static double bearingDeg(double lat1, double lon1, double lat2, double lon2);
    static double distanceKm(double lat1, double lon1, double lat2, double lon2);
    static QString compassPoint(double bearing);
};
