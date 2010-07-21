/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of libmeegotouch.
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "mstatusbar.h"

#include <QFile>
#include <QDataStream>
#include <QDir>
#include <QGraphicsSceneMouseEvent>
#include <QCoreApplication>
#include <qpainter.h>
#include <qx11info_x11.h>
#include <qgraphicsscene.h>
#include <qdebug.h>

#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QDBusConnectionInterface>

#include <X11/extensions/Xdamage.h>

static const QLatin1String PIXMAP_PROVIDER_DBUS_SERVICE("com.meego.core.MStatusBar");
static const QLatin1String PIXMAP_PROVIDER_DBUS_PATH("/statusbar");
static const QLatin1String PIXMAP_PROVIDER_DBUS_INTERFACE("com.meego.core.MStatusBar");
static const QLatin1String PIXMAP_PROVIDER_DBUS_SHAREDPIXMAP_CALL("sharedPixmapHandle");

static const QLatin1String STATUS_INDICATOR_MENU_DBUS_SERVICE("com.meego.core.MStatusIndicatorMenu");
static const QLatin1String STATUS_INDICATOR_MENU_DBUS_PATH("/statusindicatormenu");
static const QLatin1String STATUS_INDICATOR_MENU_DBUS_INTERFACE("com.meego.core.MStatusIndicatorMenu");

// for XDamage events
static int xDamageEventBase = 0;
static int xDamageErrorBase = 0;

static bool filterRegistered = false;
QCoreApplication::EventFilter oldFilter = 0;
static QHash<Damage, MDeclarativeStatusBar *> damageMap;

static int handleXError(Display *, XErrorEvent *)
{
    return 0;
}

static bool x11EventFilter(void *message, long *)
{
    XEvent *event = (XEvent *)message;

    if (event->type == xDamageEventBase + XDamageNotify) {
        XDamageNotifyEvent *xevent = (XDamageNotifyEvent *) event;

        // It is possible that the Damage has already been destroyed so register an error handler to suppress X errors
        XErrorHandler errh = XSetErrorHandler(handleXError);
        XDamageSubtract(QX11Info::display(), xevent->damage, None, None);
        XSetErrorHandler(errh);

        // notify status bar
        MDeclarativeStatusBar *statusBar = damageMap.value(xevent->damage);
        if (statusBar) {
            statusBar->update();
            return true;
        }
    }
    return false;
}



MDeclarativeStatusBar::MDeclarativeStatusBar(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    updatesEnabled(true)
{
    if (!filterRegistered) {
        ::oldFilter = QCoreApplication::instance()->setEventFilter(x11EventFilter);
        XDamageQueryExtension(QX11Info::display(), &xDamageEventBase, &xDamageErrorBase);

        filterRegistered = true;
    }

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(PIXMAP_PROVIDER_DBUS_SERVICE))
        isPixmapProviderOnline = true;
    else
        isPixmapProviderOnline = false;

    dbusWatcher = new QDBusServiceWatcher( PIXMAP_PROVIDER_DBUS_SERVICE , QDBusConnection::sessionBus(),
                                           QDBusServiceWatcher::WatchForRegistration|QDBusServiceWatcher::WatchForUnregistration,
                                           this );

    connect(dbusWatcher, SIGNAL(serviceRegistered(QString)),
            this, SLOT(handlePixmapProviderOnline()));
    connect(dbusWatcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(handlePixmapProviderOffline()));

    querySharedPixmapFromProvider();
}

MDeclarativeStatusBar::~MDeclarativeStatusBar()
{
    destroyXDamageForSharedPixmap();
}

void MDeclarativeStatusBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    qWarning() << "paint1";

    if (sharedPixmap.isNull()) {
        MDeclarativeStatusBar *view = const_cast<MDeclarativeStatusBar *>(this);
        view->querySharedPixmapFromProvider();
    }

    qWarning() << "paint2";
    if (sharedPixmap.isNull())
        return;
    qWarning() << "paint3";

    QRectF sourceRect;
//    if (controller->sceneManager()->orientation() == M::Landscape) {
//        sourceRect.setX(0);
//        sourceRect.setY(0);
//        sourceRect.setWidth(size().width());
//        sourceRect.setHeight(size().height());
//    } else {
//        sourceRect.setX(0);
//        sourceRect.setY(size().height());
//        sourceRect.setWidth(size().width());
//        sourceRect.setHeight(size().height());
//    }

    qWarning() << "drawing status bar" << sharedPixmap.size();
    painter->drawPixmap(QPointF(0.0, 0.0), sharedPixmap);// ####, sourceRect);
}

void MDeclarativeStatusBar::updateSharedPixmap()
{
    destroyXDamageForSharedPixmap();
    if ((!updatesEnabled)||(!isPixmapProviderOnline))
        return;

    if (!sharedPixmap.isNull()) {
        setupXDamageForSharedPixmap();
    }
}

void MDeclarativeStatusBar::setupXDamageForSharedPixmap()
{
    Q_ASSERT(!sharedPixmap.isNull());
    pixmapDamage = XDamageCreate(QX11Info::display(), sharedPixmap.handle(), XDamageReportNonEmpty);
    damageMap.insert(pixmapDamage, this);
}

void MDeclarativeStatusBar::destroyXDamageForSharedPixmap()
{
    if (pixmapDamage) {
        damageMap.remove(pixmapDamage);
        XDamageDestroy(QX11Info::display(), pixmapDamage);
        pixmapDamage = 0;
    }
}

void MDeclarativeStatusBar::enablePixmapUpdates()
{
    updatesEnabled = true;
    querySharedPixmapFromProvider();
}

void MDeclarativeStatusBar::disablePixmapUpdates()
{
    updatesEnabled = false;
    destroyXDamageForSharedPixmap();
}

void MDeclarativeStatusBar::querySharedPixmapFromProvider()
{
    if (!updatesEnabled || !isPixmapProviderOnline)
        return;
    QDBusInterface interface(PIXMAP_PROVIDER_DBUS_SERVICE, PIXMAP_PROVIDER_DBUS_PATH, PIXMAP_PROVIDER_DBUS_INTERFACE,
                             QDBusConnection::sessionBus());
    QDBusPendingCall asyncCall =  interface.asyncCall(PIXMAP_PROVIDER_DBUS_SHAREDPIXMAP_CALL);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(asyncCall, this);

    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(sharedPixmapHandleReceived(QDBusPendingCallWatcher*)));
}

void MDeclarativeStatusBar::sharedPixmapHandleReceived(QDBusPendingCallWatcher * call)
{
    QDBusPendingReply<quint32> reply = *call;
    if (reply.isError()) {
        qWarning() << "MDeclarativeStatusBar" << reply.error().message();
        return;
    }
    qWarning() << "handle received";
    quint32 tmp = reply;
    sharedPixmap = QPixmap::fromX11Pixmap(tmp, QPixmap::ExplicitlyShared);
    updateSharedPixmap();
    call->deleteLater();
    scene()->update(); // ###
}

void MDeclarativeStatusBar::handlePixmapProviderOnline()
{
    isPixmapProviderOnline = true;
    querySharedPixmapFromProvider();
}

void MDeclarativeStatusBar::handlePixmapProviderOffline()
{
    isPixmapProviderOnline = false;
    destroyXDamageForSharedPixmap();
}

void MDeclarativeStatusBar::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    firstPos = event->pos();
    playHapticsFeedback();
}

void MDeclarativeStatusBar::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if(firstPos.y() /* ### + style()->swipeThreshold()*/ + 15 < event->pos().y())
        showStatusIndicatorMenu();
}

void MDeclarativeStatusBar::showStatusIndicatorMenu()
{
    QDBusInterface interface(STATUS_INDICATOR_MENU_DBUS_SERVICE, STATUS_INDICATOR_MENU_DBUS_PATH, STATUS_INDICATOR_MENU_DBUS_INTERFACE, QDBusConnection::sessionBus());
    interface.call(QDBus::NoBlock, "open");
}

void MDeclarativeStatusBar::playHapticsFeedback()
{
//    style()->pressFeedback().play();
}

