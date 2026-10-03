/*
 * tst_mobile.cpp - the models behind the Qt Quick interface.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MobileApp.h"

#include <QSignalSpy>
#include <QtTest>

class MobileTest : public QObject
{
    Q_OBJECT

    static Station station(const QString &callsign)
    {
        Station s;
        s.callsign = callsign;
        s.lastHeard = 1000;
        return s;
    }

    static QString callAt(const StationModel &m, int row)
    {
        return m.data(m.index(row), StationModel::CallsignRole).toString();
    }

private slots:
    // Alphabetical, and changed in place: a newcomer is inserted where it
    // sorts and an expired station removed, never a reset that would throw
    // the view back to the top.
    void stationsAlphabeticalInPlace()
    {
        StationModel m;
        QSignalSpy resets(&m, &QAbstractItemModel::modelReset);
        QSignalSpy inserts(&m, &QAbstractItemModel::rowsInserted);
        QSignalSpy removes(&m, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changes(&m, &QAbstractItemModel::dataChanged);

        m.refresh({station("F5XYZ"), station("F1ABC-7")}, 1001, std::nullopt);
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(callAt(m, 0), QStringLiteral("F1ABC-7"));
        QCOMPARE(callAt(m, 1), QStringLiteral("F5XYZ"));
        QCOMPARE(inserts.count(), 2);

        m.refresh({station("F5XYZ"), station("F1ABC-7"), station("F4TST")}, 1002, std::nullopt);
        QCOMPARE(m.rowCount(), 3);
        QCOMPARE(callAt(m, 1), QStringLiteral("F4TST"));
        QCOMPARE(inserts.count(), 3);
        QCOMPARE(changes.count(), 2);        // the two already there, their ages

        m.refresh({station("F5XYZ"), station("F4TST")}, 1003, std::nullopt);
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(callAt(m, 0), QStringLiteral("F4TST"));
        QCOMPARE(removes.count(), 1);
        QCOMPARE(resets.count(), 0);
    }
};

QTEST_MAIN(MobileTest)
#include "tst_mobile.moc"
