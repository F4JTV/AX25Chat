/*
 * TileCache.cpp - map tiles kept on disk.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TileCache.h"

#include "Session.h"

#include <QDir>
#include <QDirIterator>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

CachingNetworkAccessManager::CachingNetworkAccessManager(qint64 maxBytes, QObject *parent)
    : QNetworkAccessManager(parent)
{
    auto *cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(directory());
    cache->setMaximumCacheSize(maxBytes);
    setCache(cache);
}

QNetworkReply *CachingNetworkAccessManager::createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    QNetworkRequest r(request);
    // Disk first, whatever the tile's age; the network only for tiles never
    // seen.  Tiles hardly change, and a map that works offline matters more
    // than one that is a month fresher.
    r.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    r.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
    r.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("AX25Chat/%1 (amateur packet radio chat; https://github.com/wb2osz/direwolf based)").arg(Session::version()));
    QNetworkReply *reply = QNetworkAccessManager::createRequest(op, r, outgoingData);
    // A failed tile says why in the system log, once per reason: a TLS
    // backend that is missing, a server refusing the request, no network.
    connect(reply, &QNetworkReply::finished, reply, [reply] {
        if (reply->error() == QNetworkReply::NoError) return;
        static QString lastReason;
        const QString reason = reply->errorString();
        if (reason == lastReason) return;
        lastReason = reason;
        qWarning("map tile %s: %s", qPrintable(reply->url().toString()), qPrintable(reason));
    });
    return reply;
}

QString CachingNetworkAccessManager::directory()
{
    // The data location rather than the cache location: the system may
    // purge caches when storage runs low, and these tiles are meant to stay.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tiles");
    QDir().mkpath(dir);
    return dir;
}

qint64 CachingNetworkAccessManager::sizeOnDisk()
{
    qint64 total = 0;
    QDirIterator it(directory(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

void CachingNetworkAccessManager::clear()
{
    QDir(directory()).removeRecursively();
    QDir().mkpath(directory());
}
