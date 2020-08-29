#include "xwindowinterface.h"
#include <QTimer>
#include <QDebug>
#include <QX11Info>
#include <QWindow>

#include <KWindowEffects>
#include <KWindowSystem>
#include <KWindowInfo>

// X11
#include <NETWM>
#include <xcb/xcb.h>
#include <xcb/shape.h>

static XWindowInterface *INSTANCE = nullptr;

XWindowInterface *XWindowInterface::instance()
{
    if (!INSTANCE)
        INSTANCE = new XWindowInterface;

    return INSTANCE;
}

XWindowInterface::XWindowInterface(QObject *parent)
    : QObject(parent)
{
    connect(KWindowSystem::self(), &KWindowSystem::windowAdded, this, &XWindowInterface::onWindowadded);
    connect(KWindowSystem::self(), &KWindowSystem::windowRemoved, this, &XWindowInterface::windowRemoved);
}

void XWindowInterface::enableBlurBehind(QWindow &view)
{
    KWindowEffects::enableBlurBehind(view.winId());
}

ApplicationItem XWindowInterface::requestInfo(quint64 wid)
{
    ApplicationItem item;
    const KWindowInfo winfo { wid, NET::WMFrameExtents
                | NET::WMWindowType
                | NET::WMGeometry
                | NET::WMDesktop
                | NET::WMState
                | NET::WMName
                | NET::WMVisibleName,
                NET::WM2WindowClass
                | NET::WM2Activities
                | NET::WM2AllowedActions
                | NET::WM2TransientFor };
    // const auto winClass = QString(winfo.windowClassName());
    const auto winClass = QString(winfo.windowClassClass().toLower());

    item.winId = wid;
    item.iconName = winClass;
    item.isActive = false;

    return item;
}

bool XWindowInterface::isAcceptableWindow(quint64 wid)
{
    QFlags<NET::WindowTypeMask> ignoreList;
    ignoreList |= NET::DesktopMask;
    ignoreList |= NET::DockMask;
    ignoreList |= NET::SplashMask;
    ignoreList |= NET::ToolbarMask;
    ignoreList |= NET::MenuMask;
    ignoreList |= NET::PopupMenuMask;
    ignoreList |= NET::NotificationMask;

    KWindowInfo info(wid, NET::WMWindowType | NET::WMState, NET::WM2TransientFor | NET::WM2WindowClass);

    if (!info.valid())
        return false;

    if (NET::typeMatchesMask(info.windowType(NET::AllTypesMask), ignoreList))
        return false;

    if (info.hasState(NET::SkipTaskbar) || info.hasState(NET::SkipPager))
        return false;

    // WM_TRANSIENT_FOR hint not set - normal window
    WId transFor = info.transientFor();
    if (transFor == 0 || transFor == wid || transFor == (WId) QX11Info::appRootWindow())
        return true;

    info = KWindowInfo(transFor, NET::WMWindowType);

    QFlags<NET::WindowTypeMask> normalFlag;
    normalFlag |= NET::NormalMask;
    normalFlag |= NET::DialogMask;
    normalFlag |= NET::UtilityMask;

    return !NET::typeMatchesMask(info.windowType(NET::AllTypesMask), normalFlag);
}

void XWindowInterface::toggleMinimize(quint64 wid)
{
    KWindowInfo info(wid, NET::WMDesktop | NET::WMState | NET::XAWMState);

    if (info.isMinimized()) {
        bool onCurrent = info.isOnDesktop(KWindowSystem::self()->currentDesktop());
        KWindowSystem::unminimizeWindow(wid);
        if (onCurrent)
            KWindowSystem::forceActiveWindow(wid);
    } else {
        KWindowSystem::minimizeWindow(wid);
    }
}

void XWindowInterface::startInitWindows()
{
    for (auto wid : KWindowSystem::self()->windows()) {
        onWindowadded(wid);
    }
}

void XWindowInterface::onWindowadded(quint64 wid)
{
    if (isAcceptableWindow(wid)) {
        emit windowAdded(wid);
    }
}
