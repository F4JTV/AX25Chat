/*
 * TileCache.h - the disk-cached network manager, handed to the QML engine.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "../model/TileCache.h"

#include <QQmlNetworkAccessManagerFactory>

class TileCacheFactory : public QQmlNetworkAccessManagerFactory
{
public:
    explicit TileCacheFactory(qint64 maxBytes) : m_maxBytes(maxBytes) {}
    QNetworkAccessManager *create(QObject *parent) override { return new CachingNetworkAccessManager(m_maxBytes, parent); }

    static QString directory() { return CachingNetworkAccessManager::directory(); }
    static qint64 sizeOnDisk() { return CachingNetworkAccessManager::sizeOnDisk(); }
    static void clear() { CachingNetworkAccessManager::clear(); }

private:
    qint64 m_maxBytes;
};
