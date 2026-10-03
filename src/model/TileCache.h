/*
 * TileCache.h - map tiles kept on disk.
 *
 * A network access manager with a disk cache under the application's data
 * folder and a request policy that prefers the cache over the network: a
 * tile that was once downloaded is served from disk from then on, expiry
 * headers or not, and the map keeps working with no connection. Only
 * tiles never seen are fetched. Both interfaces use it: the desktop map
 * widget directly, the Qt Quick one through a factory (src/mobile).
 *
 * The tile servers used (OpenStreetMap, OpenTopoMap, IGN, Esri) require an
 * identifiable User-Agent and a cache; both are set here.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QNetworkAccessManager>
#include <QString>

class CachingNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit CachingNetworkAccessManager(qint64 maxBytes, QObject *parent = nullptr);

    // Where the tiles live; created on first use.
    static QString directory();
    static qint64 sizeOnDisk();
    static void clear();

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData = nullptr) override;
};
