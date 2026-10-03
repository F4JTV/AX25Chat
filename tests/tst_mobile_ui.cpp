/*
 * tst_mobile_ui.cpp - the Qt Quick interface under real pointer events.
 *
 * Loads the QML as the application does and clicks, through QTest's
 * window-system path, where a finger would: the settings cards must open
 * their section, the header button must act.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AppConfig.h"
#include "MobileApp.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QtTest>

#include <algorithm>

class MobileUiTest : public QObject
{
    Q_OBJECT

    QQmlApplicationEngine *m_engine = nullptr;
    MobileApp *m_app = nullptr;
    QQuickWindow *m_window = nullptr;

    QObject *stack() { return m_window->findChild<QObject *>(QStringLiteral("stack")); }
    int depth() { return stack() ? stack()->property("depth").toInt() : -1; }

    // The window coordinates of an item's centre.
    QPoint centreOf(QQuickItem *item)
    {
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    }

    // The visual tree, which the QObject tree does not always mirror.
    static void collect(QQuickItem *item, const char *classPrefix, QList<QQuickItem *> &out)
    {
        if (QString::fromLatin1(item->metaObject()->className()).startsWith(QLatin1String(classPrefix))) out << item;
        for (QQuickItem *child : item->childItems()) collect(child, classPrefix, out);
    }

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Material"));
        AppConfig config;
        config.station.callsign = QStringLiteral("F4TST");
        config.modem.autoStart = false;
        m_app = new MobileApp(config, this);
        m_engine = new QQmlApplicationEngine(this);
        m_engine->addImageProvider(QStringLiteral("symbols"), new SymbolImageProvider);
        m_engine->rootContext()->setContextProperty(QStringLiteral("app"), m_app);
        m_engine->load(QUrl::fromLocalFile(QStringLiteral(AX25CHAT_QML_DIR "/Main.qml")));
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        QVERIFY(m_window);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
    }

    void settingsCardOpensItsSection()
    {
        QMetaObject::invokeMethod(m_window, "showPage", Q_ARG(QVariant, QStringLiteral("settings")));
        QTRY_COMPARE(depth(), 2);
        QTest::qWait(400);   // the push transition
        // The first card of the list: the item that has a MouseArea child
        // under the settings page.
        QQuickItem *settingsPage = qobject_cast<QQuickItem *>(stack()->property("currentItem").value<QObject *>());
        QVERIFY(settingsPage);
        // The cards are Panes; the topmost one is clicked where a finger
        // would land, whatever handles the tap inside.
        QList<QQuickItem *> cards;
        collect(settingsPage, "Pane", cards);
        QVERIFY2(!cards.isEmpty(), "no card in the settings list");
        std::sort(cards.begin(), cards.end(), [this](QQuickItem *a, QQuickItem *b) { return centreOf(a).y() < centreOf(b).y(); });
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centreOf(cards.first()));
        QTRY_COMPARE(depth(), 3);
        QCOMPARE(stack()->property("currentItem").value<QObject *>()->property("pageName").toString(), QStringLiteral("settings-section"));
    }

    void headerButtonTogglesTheStation()
    {
        const bool before = m_app->session()->stationOn();
        QQuickItem *button = nullptr;
        QList<QQuickItem *> buttons;
        // The header is the window's own, outside the content item.
        // Every item in the window: a control written in QML (the style's
        // ToolButton.qml) has a generated class name, ToolButton_QMLTYPE_N.
        collect(m_window->contentItem(), "", buttons);
        for (QQuickItem *item : buttons) {
            const QString text = item->property("text").toString();
            const QString cls = QString::fromLatin1(item->metaObject()->className());
            if (cls.contains(QLatin1String("ToolButton")) && (text == QLatin1String("Start") || text == QLatin1String("Stop"))) { button = item; break; }
        }
        QVERIFY(button);
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, centreOf(button));
        QTRY_VERIFY(m_app->session()->stationOn() != before);
        m_app->session()->stopStation();
    }
};

QTEST_MAIN(MobileUiTest)
#include "tst_mobile_ui.moc"
