/*
 * GpsProvider.cpp - where the current position comes from.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GpsProvider.h"

#ifdef AX25CHAT_HAVE_POSITIONING
#include "PhoneLocation.h"
#endif

SerialGpsProvider::SerialGpsProvider(QObject *parent)
    : GpsProvider(parent), m_receiver(new GpsReceiver(this))
{
    connect(m_receiver, &GpsReceiver::fixChanged, this, &GpsProvider::fixChanged);
    connect(m_receiver, &GpsReceiver::statusChanged, this, &GpsProvider::statusChanged);
    connect(m_receiver, &GpsReceiver::logMessage, this, &GpsProvider::logMessage);
}

GpsProvider *GpsProvider::create(QObject *parent)
{
#if defined(Q_OS_ANDROID) && defined(AX25CHAT_HAVE_POSITIONING)
    return new PhoneLocation(parent);
#else
    return new SerialGpsProvider(parent);
#endif
}
