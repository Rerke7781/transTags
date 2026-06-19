#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLockFile>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QScreen>
#include <QSocketNotifier>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr unsigned long kOpacityFull = 0xffffffffUL;
constexpr unsigned long kOpacityMin = 0x19191919UL;
constexpr int kMaxTrackedWindows = 64;
constexpr long kNetWmStateRemove = 0;
constexpr long kNetWmStateAdd = 1;

int g_signalSockets[2] = {-1, -1};

void handleUnixSignal(int)
{
    const char byte = 1;
    if (g_signalSockets[0] != -1) {
        const ssize_t written = ::write(g_signalSockets[0], &byte, sizeof(byte));
        (void)written;
    }
}

int xErrorHandler(Display *display, XErrorEvent *event)
{
    char message[256] = {};
    XGetErrorText(display, event->error_code, message, sizeof(message));
    std::fprintf(stderr, "transTags X11 warning: %s (request %d, resource 0x%lx)\n",
                 message, event->request_code, event->resourceid);
    return 0;
}

QIcon createFallbackIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(42, 132, 255));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(8, 10, 48, 42, 8, 8);
    painter.setBrush(QColor(255, 255, 255, 190));
    painter.drawRoundedRect(16, 18, 32, 26, 5, 5);
    painter.setBrush(QColor(28, 28, 30));
    painter.drawRect(22, 50, 20, 4);
    painter.end();

    return QIcon(pixmap);
}

QString sessionType()
{
    const QByteArray value = qgetenv("XDG_SESSION_TYPE");
    return value.isEmpty() ? QStringLiteral("unknown") : QString::fromLocal8Bit(value);
}

} // namespace

struct WindowState {
    Window window = None;
    unsigned long opacity = kOpacityFull;
    bool clickThrough = false;
    bool pinned = false;
    std::vector<Window> shapedWindows;
};

class WindowController final : public QObject {
    Q_OBJECT

public:
    explicit WindowController(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            m_error = tr("无法打开 X11 Display。请在 Ubuntu on Xorg 会话运行，或从桌面终端启动。");
            return;
        }

        m_screen = DefaultScreen(m_display);
        m_root = RootWindow(m_display, m_screen);
        initAtoms();
        resetMarkedInputShapes();
        m_available = true;
    }

    ~WindowController() override
    {
        cleanup();
        if (m_display)
            XCloseDisplay(m_display);
    }

    bool available() const { return m_available; }
    QString errorString() const { return m_error; }

    void setTransparencyStep(int value)
    {
        m_transparencyStep = std::clamp(value, 1, 50);
    }

    void setClickThroughEnabled(bool value)
    {
        m_clickThroughEnabled = value;
        if (!value)
            unlockAllClickThrough();
    }

    void setPinEnabled(bool value)
    {
        m_pinEnabled = value;
        if (!value)
            unlockAllPinned();
    }

public slots:
    void adjustTransparencyUp()
    {
        adjustTransparency(true);
    }

    void adjustTransparencyDown()
    {
        adjustTransparency(false);
    }

    void adjustTransparency(bool moreTransparent)
    {
        if (!m_available)
            return;

        const Window window = actionTargetWindow();
        if (!isValidWindow(window)) {
            emit statusChanged(tr("没有找到可操作的活动窗口"));
            return;
        }

        WindowState *state = stateFor(window, true);
        if (!state)
            return;

        const unsigned long step = kOpacityFull * static_cast<unsigned long>(m_transparencyStep) / 100UL;
        unsigned long opacity = state->opacity;

        if (moreTransparent)
            opacity = opacity > kOpacityMin + step ? opacity - step : kOpacityMin;
        else
            opacity = opacity + step < kOpacityFull ? opacity + step : kOpacityFull;

        setWindowOpacity(window, opacity);
        if (m_clickThroughEnabled)
            setClickThrough(window, opacity < kOpacityFull);

        emit statusChanged(tr("%1：透明度 %2%3")
                           .arg(windowTitle(window))
                           .arg(qRound(static_cast<double>(opacity) * 100.0 / static_cast<double>(kOpacityFull)))
                           .arg(opacity < kOpacityFull && m_clickThroughEnabled ? tr("，鼠标穿透已开启") : QString()));
    }

    void toggleClickThrough()
    {
        Window window = lastClickThroughWindow();
        if (isValidWindow(window)) {
            setClickThrough(window, false);
            emit statusChanged(tr("%1：鼠标穿透已解除").arg(windowTitle(window)));
            return;
        }

        window = actionTargetWindow();
        if (!isValidWindow(window)) {
            emit statusChanged(tr("没有可切换穿透的窗口"));
            return;
        }

        WindowState *state = stateFor(window, true);
        const bool enable = state && !state->clickThrough;
        setClickThrough(window, enable);
        emit statusChanged(tr("%1：%2").arg(windowTitle(window), enable ? tr("鼠标穿透已开启") : tr("鼠标穿透已解除")));
    }

    void unlockAllClickThrough()
    {
        for (int i = static_cast<int>(m_states.size()) - 1; i >= 0; --i) {
            if (m_states[static_cast<size_t>(i)].clickThrough)
                setClickThrough(m_states[static_cast<size_t>(i)].window, false);
        }
    }

    void togglePin()
    {
        if (!m_pinEnabled) {
            emit statusChanged(tr("窗口置顶功能未启用"));
            return;
        }

        const Window window = pinTargetWindow();
        if (!isValidWindow(window)) {
            emit statusChanged(tr("没有找到可置顶的窗口"));
            return;
        }

        WindowState *state = stateFor(window, true);
        const bool enable = state && !state->pinned;
        setPinned(window, enable);
        emit statusChanged(tr("%1：%2").arg(windowTitle(window), enable ? tr("已锁定到最上层") : tr("已取消置顶")));
    }

    void unlockAllPinned()
    {
        for (int i = static_cast<int>(m_states.size()) - 1; i >= 0; --i) {
            if (m_states[static_cast<size_t>(i)].pinned)
                setPinned(m_states[static_cast<size_t>(i)].window, false);
        }
    }

    void centerWindow()
    {
        if (!m_available)
            return;

        const Window window = actionTargetWindow();
        if (!isValidWindow(window)) {
            emit statusChanged(tr("没有找到可居中的活动窗口"));
            return;
        }

        XWindowAttributes attrs = {};
        if (!XGetWindowAttributes(m_display, window, &attrs))
            return;

        long workX = 0;
        long workY = 0;
        long workW = DisplayWidth(m_display, m_screen);
        long workH = DisplayHeight(m_display, m_screen);
        readWorkArea(workX, workY, workW, workH);

        const int x = static_cast<int>(workX + (workW - attrs.width) / 2);
        const int y = static_cast<int>(workY + (workH - attrs.height) / 2);
        XMoveWindow(m_display, window, x, y);
        XFlush(m_display);
        emit statusChanged(tr("%1：已居中").arg(windowTitle(window)));
    }

    void cleanup()
    {
        if (!m_available || m_cleaned)
            return;

        m_cleaned = true;
        for (int i = static_cast<int>(m_states.size()) - 1; i >= 0; --i) {
            const Window window = m_states[static_cast<size_t>(i)].window;
            if (!isValidWindow(window))
                continue;

            if (m_states[static_cast<size_t>(i)].clickThrough) {
                for (Window shapedWindow : m_states[static_cast<size_t>(i)].shapedWindows)
                    resetInputShape(shapedWindow);
            }

            if (m_states[static_cast<size_t>(i)].pinned)
                sendWmState(window, kNetWmStateRemove, m_atomWmStateAbove);
        }

        XFlush(m_display);
    }

signals:
    void statusChanged(const QString &message);

private:
    void initAtoms()
    {
        m_atomActiveWindow = XInternAtom(m_display, "_NET_ACTIVE_WINDOW", False);
        m_atomOpacity = XInternAtom(m_display, "_NET_WM_WINDOW_OPACITY", False);
        m_atomWmState = XInternAtom(m_display, "_NET_WM_STATE", False);
        m_atomWmStateLegacy = XInternAtom(m_display, "WM_STATE", False);
        m_atomWmStateAbove = XInternAtom(m_display, "_NET_WM_STATE_ABOVE", False);
        m_atomWorkArea = XInternAtom(m_display, "_NET_WORKAREA", False);
        m_atomCurrentDesktop = XInternAtom(m_display, "_NET_CURRENT_DESKTOP", False);
        m_atomNetWmName = XInternAtom(m_display, "_NET_WM_NAME", False);
        m_atomUtf8String = XInternAtom(m_display, "UTF8_STRING", False);
        m_atomWmClass = XInternAtom(m_display, "WM_CLASS", False);
        m_atomClickThroughMarker = XInternAtom(m_display, "TRANSTAGS_CLICK_THROUGH", False);
    }

    bool isValidWindow(Window window) const
    {
        if (window == None || window == m_root)
            return false;

        XWindowAttributes attrs = {};
        if (!XGetWindowAttributes(m_display, window, &attrs))
            return false;

        return attrs.c_class == InputOutput && attrs.map_state != IsUnmapped && !isOwnWindowOrAncestor(window);
    }

    Window actionTargetWindow()
    {
        const Window active = activeWindow();
        if (isValidWindow(active))
            return active;

        const Window pointer = windowUnderPointer();
        if (isValidWindow(pointer))
            return pointer;

        if (isValidWindow(m_lastTargetWindow))
            return m_lastTargetWindow;

        return None;
    }

    Window activeWindow() const
    {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *data = nullptr;
        Window window = None;

        if (XGetWindowProperty(m_display, m_root, m_atomActiveWindow, 0, 1, False, XA_WINDOW,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &data) == Success &&
            data && nitems == 1) {
            window = *reinterpret_cast<Window *>(data);
        }

        if (data)
            XFree(data);

        if (!isValidWindow(window)) {
            int revert = 0;
            XGetInputFocus(m_display, &window, &revert);
        }

        window = clientWindowFor(window);
        return isValidWindow(window) ? window : None;
    }

    Window windowUnderPointer() const
    {
        Window root = None;
        Window child = None;
        int rootX = 0;
        int rootY = 0;
        int winX = 0;
        int winY = 0;
        unsigned int mask = 0;

        if (!XQueryPointer(m_display, m_root, &root, &child, &rootX, &rootY, &winX, &winY, &mask) || child == None)
            return None;

        Window current = child;
        for (int depth = 0; depth < 32; ++depth) {
            Window next = None;
            if (!XQueryPointer(m_display, current, &root, &next, &rootX, &rootY, &winX, &winY, &mask) || next == None)
                break;
            current = next;
        }

        return clientWindowFor(current);
    }

    Window lastClickThroughWindow()
    {
        for (int i = static_cast<int>(m_recentClickThrough.size()) - 1; i >= 0; --i) {
            const Window window = m_recentClickThrough[static_cast<size_t>(i)];
            WindowState *state = stateFor(window, false);
            if (state && state->clickThrough && isValidWindow(window))
                return window;

            m_recentClickThrough.erase(m_recentClickThrough.begin() + i);
        }

        return None;
    }

    Window pinTargetWindow()
    {
        const Window clickThroughWindow = lastClickThroughWindow();
        return clickThroughWindow != None ? clickThroughWindow : actionTargetWindow();
    }

    WindowState *stateFor(Window window, bool create)
    {
        for (auto &state : m_states) {
            if (state.window == window)
                return &state;
        }

        if (!create || m_states.size() >= kMaxTrackedWindows)
            return nullptr;

        WindowState state;
        state.window = window;
        m_states.push_back(state);
        return &m_states.back();
    }

    bool isOwnWindow(Window window) const
    {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *data = nullptr;
        bool own = false;

        if (XGetWindowProperty(m_display, window, m_atomWmClass, 0, 64, False,
                               XA_STRING, &actualType, &actualFormat, &nitems, &bytesAfter, &data) == Success &&
            data) {
            QByteArray wmClass(reinterpret_cast<char *>(data), static_cast<int>(nitems));
            own = wmClass.toLower().contains("transtags") || wmClass.toLower().contains("transtagsqt");
        }

        if (data)
            XFree(data);

        return own;
    }

    bool isOwnWindowOrAncestor(Window window) const
    {
        Window current = window;
        for (int depth = 0; depth < 32 && current != None && current != m_root; ++depth) {
            if (isOwnWindow(current))
                return true;

            Window root = None;
            Window parent = None;
            Window *children = nullptr;
            unsigned int childCount = 0;
            if (!XQueryTree(m_display, current, &root, &parent, &children, &childCount))
                return false;
            if (children)
                XFree(children);
            current = parent;
        }

        return false;
    }

    bool hasProperty(Window window, Atom atom) const
    {
        if (window == None || atom == None)
            return false;

        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *data = nullptr;
        const int result = XGetWindowProperty(m_display, window, atom, 0, 0, False, AnyPropertyType,
                                              &actualType, &actualFormat, &nitems, &bytesAfter, &data);
        if (data)
            XFree(data);

        return result == Success && actualType != None;
    }

    Window clientWindowFor(Window window) const
    {
        if (window == None || window == m_root)
            return None;

        Window current = window;
        Window fallback = window;
        for (int depth = 0; depth < 32 && current != None && current != m_root; ++depth) {
            if (hasProperty(current, m_atomWmStateLegacy))
                return current;

            fallback = current;
            Window root = None;
            Window parent = None;
            Window *children = nullptr;
            unsigned int childCount = 0;
            if (!XQueryTree(m_display, current, &root, &parent, &children, &childCount))
                break;
            if (children)
                XFree(children);
            current = parent;
        }

        return fallback;
    }

    bool windowExists(Window window) const
    {
        if (window == None)
            return false;

        XWindowAttributes attrs = {};
        return XGetWindowAttributes(m_display, window, &attrs) != 0;
    }

    void collectDescendants(Window window, std::vector<Window> &windows, std::set<Window> &seen) const
    {
        if (window == None || seen.count(window))
            return;

        seen.insert(window);
        windows.push_back(window);

        Window root = None;
        Window parent = None;
        Window *children = nullptr;
        unsigned int childCount = 0;
        if (!XQueryTree(m_display, window, &root, &parent, &children, &childCount))
            return;

        for (unsigned int i = 0; i < childCount; ++i)
            collectDescendants(children[i], windows, seen);

        if (children)
            XFree(children);
    }

    std::vector<Window> inputShapeWindows(Window window) const
    {
        std::vector<Window> windows;
        std::set<Window> seen;

        collectDescendants(window, windows, seen);

        Window current = window;
        for (int depth = 0; depth < 4; ++depth) {
            Window root = None;
            Window parent = None;
            Window *children = nullptr;
            unsigned int childCount = 0;
            if (!XQueryTree(m_display, current, &root, &parent, &children, &childCount))
                break;
            if (children)
                XFree(children);
            if (parent == None || parent == m_root || seen.count(parent))
                break;

            collectDescendants(parent, windows, seen);
            current = parent;
        }

        return windows;
    }

    void clearInputShape(Window window)
    {
        if (!windowExists(window))
            return;

        unsigned long marker = 1;
        XChangeProperty(m_display, window, m_atomClickThroughMarker, XA_CARDINAL, 32,
                        PropModeReplace, reinterpret_cast<unsigned char *>(&marker), 1);

        XserverRegion emptyRegion = XFixesCreateRegion(m_display, nullptr, 0);
        XFixesSetWindowShapeRegion(m_display, window, ShapeInput, 0, 0, emptyRegion);
        XFixesDestroyRegion(m_display, emptyRegion);
    }

    void resetInputShape(Window window)
    {
        if (!windowExists(window))
            return;

        XFixesSetWindowShapeRegion(m_display, window, ShapeInput, 0, 0, None);
        XDeleteProperty(m_display, window, m_atomClickThroughMarker);
    }

    void resetMarkedInputShapes()
    {
        std::set<Window> seen;
        resetMarkedInputShapes(m_root, seen);
        XFlush(m_display);
    }

    void resetMarkedInputShapes(Window window, std::set<Window> &seen)
    {
        if (window == None || seen.count(window))
            return;

        seen.insert(window);
        if (hasProperty(window, m_atomClickThroughMarker))
            resetInputShape(window);

        Window root = None;
        Window parent = None;
        Window *children = nullptr;
        unsigned int childCount = 0;
        if (!XQueryTree(m_display, window, &root, &parent, &children, &childCount))
            return;

        for (unsigned int i = 0; i < childCount; ++i)
            resetMarkedInputShapes(children[i], seen);

        if (children)
            XFree(children);
    }

    void dropStateIfUnused(Window window)
    {
        auto it = std::find_if(m_states.begin(), m_states.end(), [window](const WindowState &state) {
            return state.window == window;
        });

        if (it == m_states.end())
            return;

        if (it->opacity < kOpacityFull || it->clickThrough || it->pinned)
            return;

        m_states.erase(it);
    }

    QString windowTitle(Window window) const
    {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *data = nullptr;

        if (XGetWindowProperty(m_display, window, m_atomNetWmName, 0, 255, False, m_atomUtf8String,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &data) == Success &&
            data) {
            const QString title = QString::fromUtf8(reinterpret_cast<char *>(data));
            XFree(data);
            if (!title.isEmpty())
                return title;
        }

        if (data)
            XFree(data);

        XTextProperty text = {};
        if (XGetWMName(m_display, window, &text) && text.value) {
            const QString title = QString::fromLocal8Bit(reinterpret_cast<char *>(text.value));
            XFree(text.value);
            if (!title.isEmpty())
                return title;
        }

        return QStringLiteral("0x%1").arg(static_cast<qulonglong>(window), 0, 16);
    }

    void setWindowOpacity(Window window, unsigned long opacity)
    {
        WindowState *state = stateFor(window, true);
        if (!state)
            return;

        state->opacity = opacity;
        if (opacity >= kOpacityFull) {
            XDeleteProperty(m_display, window, m_atomOpacity);
            state->opacity = kOpacityFull;
        } else {
            XChangeProperty(m_display, window, m_atomOpacity, XA_CARDINAL, 32,
                            PropModeReplace, reinterpret_cast<unsigned char *>(&opacity), 1);
        }

        m_lastTargetWindow = window;
        XFlush(m_display);
        dropStateIfUnused(window);
    }

    void setClickThrough(Window window, bool enable)
    {
        if (!isValidWindow(window))
            return;

        WindowState *state = stateFor(window, true);
        if (!state)
            return;

        if (enable) {
            state->shapedWindows = inputShapeWindows(window);
            for (Window shapedWindow : state->shapedWindows)
                clearInputShape(shapedWindow);
            state->clickThrough = true;
            rememberClickThrough(window);
        } else {
            for (Window shapedWindow : state->shapedWindows)
                resetInputShape(shapedWindow);
            state->shapedWindows.clear();
            state->clickThrough = false;
            forgetClickThrough(window);
        }

        m_lastTargetWindow = window;
        XFlush(m_display);
        dropStateIfUnused(window);
    }

    void rememberClickThrough(Window window)
    {
        forgetClickThrough(window);
        m_recentClickThrough.push_back(window);
    }

    void forgetClickThrough(Window window)
    {
        m_recentClickThrough.erase(std::remove(m_recentClickThrough.begin(), m_recentClickThrough.end(), window),
                                   m_recentClickThrough.end());
    }

    void sendWmState(Window window, long action, Atom stateAtom)
    {
        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.display = m_display;
        event.xclient.window = window;
        event.xclient.message_type = m_atomWmState;
        event.xclient.format = 32;
        event.xclient.data.l[0] = action;
        event.xclient.data.l[1] = static_cast<long>(stateAtom);
        event.xclient.data.l[2] = 0;
        event.xclient.data.l[3] = 1;
        event.xclient.data.l[4] = 0;

        XSendEvent(m_display, m_root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(m_display);
    }

    void setPinned(Window window, bool enable)
    {
        if (!isValidWindow(window))
            return;

        WindowState *state = stateFor(window, true);
        if (!state)
            return;

        sendWmState(window, enable ? kNetWmStateAdd : kNetWmStateRemove, m_atomWmStateAbove);
        if (enable)
            XRaiseWindow(m_display, window);

        state->pinned = enable;
        m_lastTargetWindow = window;
        XFlush(m_display);
        dropStateIfUnused(window);
    }

    bool readWorkArea(long &x, long &y, long &width, long &height) const
    {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *data = nullptr;
        unsigned long desktop = 0;

        if (XGetWindowProperty(m_display, m_root, m_atomCurrentDesktop, 0, 1, False, XA_CARDINAL,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &data) == Success &&
            data && nitems == 1) {
            desktop = *reinterpret_cast<unsigned long *>(data);
        }

        if (data) {
            XFree(data);
            data = nullptr;
        }

        if (XGetWindowProperty(m_display, m_root, m_atomWorkArea, 0, 4 * 32, False, XA_CARDINAL,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &data) != Success ||
            !data || nitems < (desktop + 1) * 4) {
            if (data)
                XFree(data);
            return false;
        }

        auto *workAreas = reinterpret_cast<unsigned long *>(data);
        const unsigned long offset = desktop * 4;
        x = static_cast<long>(workAreas[offset]);
        y = static_cast<long>(workAreas[offset + 1]);
        width = static_cast<long>(workAreas[offset + 2]);
        height = static_cast<long>(workAreas[offset + 3]);
        XFree(data);
        return true;
    }

    Display *m_display = nullptr;
    Window m_root = None;
    int m_screen = 0;
    bool m_available = false;
    bool m_cleaned = false;
    QString m_error;
    int m_transparencyStep = 10;
    bool m_clickThroughEnabled = true;
    bool m_pinEnabled = true;
    Window m_lastTargetWindow = None;
    std::vector<WindowState> m_states;
    std::vector<Window> m_recentClickThrough;

    Atom m_atomActiveWindow = None;
    Atom m_atomOpacity = None;
    Atom m_atomWmState = None;
    Atom m_atomWmStateLegacy = None;
    Atom m_atomWmStateAbove = None;
    Atom m_atomWorkArea = None;
    Atom m_atomCurrentDesktop = None;
    Atom m_atomNetWmName = None;
    Atom m_atomUtf8String = None;
    Atom m_atomWmClass = None;
    Atom m_atomClickThroughMarker = None;
};

class HotkeyThread final : public QThread {
    Q_OBJECT

public:
    explicit HotkeyThread(QObject *parent = nullptr)
        : QThread(parent)
    {
    }

    ~HotkeyThread() override
    {
        requestInterruption();
        wait(1000);
    }

signals:
    void transparencyUp();
    void transparencyDown();
    void toggleClickThrough();
    void togglePin();
    void centerWindow();
    void statusChanged(const QString &message);

protected:
    void run() override
    {
        Display *display = XOpenDisplay(nullptr);
        if (!display) {
            emit statusChanged(tr("热键监听失败：无法打开 X11 Display"));
            return;
        }

        const Window root = DefaultRootWindow(display);
        const unsigned int numLock = numLockMask(display);

        const KeyCode keyLeft = XKeysymToKeycode(display, XK_Left);
        const KeyCode keyRight = XKeysymToKeycode(display, XK_Right);
        const KeyCode keyUp = XKeysymToKeycode(display, XK_Up);
        const KeyCode keyDown = XKeysymToKeycode(display, XK_Down);
        const KeyCode keyNumpad5 = XKeysymToKeycode(display, XK_KP_5);

        grabKey(display, root, keyLeft, Mod1Mask, numLock);
        grabKey(display, root, keyRight, Mod1Mask, numLock);
        grabKey(display, root, keyUp, Mod1Mask, numLock);
        grabKey(display, root, keyDown, Mod1Mask, numLock);
        grabKey(display, root, keyNumpad5, ControlMask, numLock);
        XFlush(display);

        emit statusChanged(tr("热键已注册：Alt+←/→、Alt+↑、Alt+↓、Ctrl+小键盘5"));

        const int fd = ConnectionNumber(display);
        while (!isInterruptionRequested()) {
            while (XPending(display) > 0) {
                XEvent event = {};
                XNextEvent(display, &event);
                if (event.type != KeyPress)
                    continue;

                const unsigned int state = event.xkey.state & ~(LockMask | numLock);
                const KeyCode key = static_cast<KeyCode>(event.xkey.keycode);

                if (key == keyLeft && (state & Mod1Mask) == Mod1Mask)
                    emit transparencyUp();
                else if (key == keyRight && (state & Mod1Mask) == Mod1Mask)
                    emit transparencyDown();
                else if (key == keyUp && (state & Mod1Mask) == Mod1Mask)
                    emit toggleClickThrough();
                else if (key == keyDown && (state & Mod1Mask) == Mod1Mask)
                    emit togglePin();
                else if (key == keyNumpad5 && (state & ControlMask) == ControlMask)
                    emit centerWindow();
            }

            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            timeval timeout = {};
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            select(fd + 1, &fds, nullptr, nullptr, &timeout);
        }

        XUngrabKey(display, AnyKey, AnyModifier, root);
        XFlush(display);
        XCloseDisplay(display);
    }

private:
    static unsigned int numLockMask(Display *display)
    {
        unsigned int mask = 0;
        XModifierKeymap *modmap = XGetModifierMapping(display);
        if (!modmap)
            return 0;

        const KeyCode numLock = XKeysymToKeycode(display, XK_Num_Lock);
        for (int mod = 0; mod < 8; ++mod) {
            for (int key = 0; key < modmap->max_keypermod; ++key) {
                const KeyCode code = modmap->modifiermap[mod * modmap->max_keypermod + key];
                if (code == numLock)
                    mask = 1U << mod;
            }
        }

        XFreeModifiermap(modmap);
        return mask;
    }

    static void grabKey(Display *display, Window root, KeyCode keycode, unsigned int modifiers, unsigned int numLock)
    {
        const std::array<unsigned int, 4> variants = {
            0U,
            LockMask,
            numLock,
            numLock | LockMask,
        };

        for (const unsigned int variant : variants) {
            XGrabKey(display, keycode, modifiers | variant, root, True, GrabModeAsync, GrabModeAsync);
        }
    }
};

class ToastPopup final : public QWidget {
    Q_OBJECT

public:
    explicit ToastPopup(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 10, 16, 10);

        m_label = new QLabel(this);
        m_label->setStyleSheet(QStringLiteral("color: white; font-size: 14px;"));
        m_label->setTextFormat(Qt::PlainText);
        m_label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        layout->addWidget(m_label);

        m_timer.setSingleShot(true);
        m_timer.setInterval(1800);
        connect(&m_timer, &QTimer::timeout, this, &QWidget::hide);
    }

public slots:
    void showMessage(const QString &message)
    {
        if (message.trimmed().isEmpty())
            return;

        m_label->setText(message);
        adjustSize();
        resize(qBound(220, width(), 560), qMax(height(), 44));

        const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1024, 768);
        move(area.right() - width() - 20, area.bottom() - height() - 20);

        show();
        raise();
        m_timer.start();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QColor(80, 80, 80, 220));
        painter.setBrush(QColor(35, 35, 35, 230));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 10, 10);
    }

private:
    QLabel *m_label = nullptr;
    QTimer m_timer;
};

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(QSettings &settings, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_settings(settings)
    {
        setWindowTitle(tr("transTags Linux 设置"));
        setWindowFlags((windowFlags()
                        | Qt::CustomizeWindowHint
                        | Qt::WindowTitleHint
                        | Qt::WindowCloseButtonHint)
                       & ~Qt::WindowMinimizeButtonHint
                       & ~Qt::WindowMaximizeButtonHint
                       & ~Qt::WindowContextHelpButtonHint);
        setMinimumWidth(460);

        auto *root = new QVBoxLayout(this);

        auto *sessionLabel = new QLabel(this);
        sessionLabel->setWordWrap(true);
        if (sessionType() == QStringLiteral("x11")) {
            sessionLabel->setText(tr("当前会话：X11。完整功能可用。"));
        } else {
            sessionLabel->setText(tr("当前会话：%1。普通 Qt 程序无法完整控制 Wayland 原生窗口；请在登录界面选择 Ubuntu on Xorg 后使用。").arg(sessionType()));
            sessionLabel->setStyleSheet(QStringLiteral("color: #b45309;"));
        }
        root->addWidget(sessionLabel);

        auto *hotkeyBox = new QGroupBox(tr("快捷键"), this);
        auto *hotkeyLayout = new QGridLayout(hotkeyBox);
        hotkeyLayout->addWidget(new QLabel(tr("Alt + ← / →"), this), 0, 0);
        hotkeyLayout->addWidget(new QLabel(tr("调整当前窗口透明度，并自动开关鼠标穿透"), this), 0, 1);
        hotkeyLayout->addWidget(new QLabel(tr("Alt + ↑"), this), 1, 0);
        hotkeyLayout->addWidget(new QLabel(tr("切换鼠标穿透；有穿透窗口时解除，没有时给当前窗口加穿透"), this), 1, 1);
        hotkeyLayout->addWidget(new QLabel(tr("Alt + ↓"), this), 2, 0);
        hotkeyLayout->addWidget(new QLabel(tr("锁定/取消窗口置顶，优先作用于最近穿透窗口"), this), 2, 1);
        hotkeyLayout->addWidget(new QLabel(tr("Ctrl + 小键盘 5"), this), 3, 0);
        hotkeyLayout->addWidget(new QLabel(tr("窗口居中"), this), 3, 1);
        root->addWidget(hotkeyBox);

        auto *settingsBox = new QGroupBox(tr("功能"), this);
        auto *settingsLayout = new QVBoxLayout(settingsBox);

        m_clickThroughCheck = new QCheckBox(tr("透明后启用鼠标穿透"), this);
        m_clickThroughCheck->setChecked(m_settings.value(QStringLiteral("clickThrough"), true).toBool());
        settingsLayout->addWidget(m_clickThroughCheck);

        m_pinCheck = new QCheckBox(tr("启用窗口置顶锁定"), this);
        m_pinCheck->setChecked(m_settings.value(QStringLiteral("pin"), true).toBool());
        settingsLayout->addWidget(m_pinCheck);

        auto *sliderRow = new QHBoxLayout();
        sliderRow->addWidget(new QLabel(tr("透明度步长"), this));
        m_stepSlider = new QSlider(Qt::Horizontal, this);
        m_stepSlider->setRange(1, 50);
        m_stepSlider->setValue(m_settings.value(QStringLiteral("transparencyStep"), 10).toInt());
        sliderRow->addWidget(m_stepSlider, 1);
        m_stepLabel = new QLabel(this);
        sliderRow->addWidget(m_stepLabel);
        settingsLayout->addLayout(sliderRow);
        root->addWidget(settingsBox);

        m_statusLabel = new QLabel(tr("就绪"), this);
        m_statusLabel->setWordWrap(true);
        root->addWidget(m_statusLabel);

        auto *buttonRow = new QHBoxLayout();
        auto *unlockButton = new QPushButton(tr("切换穿透"), this);
        auto *unpinButton = new QPushButton(tr("取消全部置顶"), this);
        buttonRow->addWidget(unlockButton);
        buttonRow->addWidget(unpinButton);
        buttonRow->addStretch(1);
        root->addLayout(buttonRow);

        connect(m_stepSlider, &QSlider::valueChanged, this, [this](int value) {
            m_stepLabel->setText(tr("%1%").arg(value));
            m_settings.setValue(QStringLiteral("transparencyStep"), value);
            emit transparencyStepChanged(value);
        });
        connect(m_clickThroughCheck, &QCheckBox::toggled, this, [this](bool enabled) {
            m_settings.setValue(QStringLiteral("clickThrough"), enabled);
            emit clickThroughChanged(enabled);
        });
        connect(m_pinCheck, &QCheckBox::toggled, this, [this](bool enabled) {
            m_settings.setValue(QStringLiteral("pin"), enabled);
            emit pinChanged(enabled);
        });
        connect(unlockButton, &QPushButton::clicked, this, &SettingsDialog::toggleClickThroughRequested);
        connect(unpinButton, &QPushButton::clicked, this, &SettingsDialog::unlockAllPinnedRequested);

        m_stepLabel->setText(tr("%1%").arg(m_stepSlider->value()));
    }

    int transparencyStep() const { return m_stepSlider->value(); }
    bool clickThroughEnabled() const { return m_clickThroughCheck->isChecked(); }
    bool pinEnabled() const { return m_pinCheck->isChecked(); }

public slots:
    void setStatus(const QString &message)
    {
        m_statusLabel->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss ")) + message);
    }

signals:
    void transparencyStepChanged(int value);
    void clickThroughChanged(bool enabled);
    void pinChanged(bool enabled);
    void toggleClickThroughRequested();
    void unlockAllPinnedRequested();

protected:
    void closeEvent(QCloseEvent *event) override
    {
        hide();
        event->ignore();
    }

private:
    QSettings &m_settings;
    QCheckBox *m_clickThroughCheck = nullptr;
    QCheckBox *m_pinCheck = nullptr;
    QSlider *m_stepSlider = nullptr;
    QLabel *m_stepLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
};

int main(int argc, char **argv)
{
    XInitThreads();
    XSetErrorHandler(xErrorHandler);

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setApplicationName(QStringLiteral("transTagsLinux"));
    QApplication::setOrganizationName(QStringLiteral("transTags"));

    QSocketNotifier *signalNotifier = nullptr;
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalSockets) == 0) {
        signalNotifier = new QSocketNotifier(g_signalSockets[1], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, [&app]() {
            char buffer[16] = {};
            const ssize_t bytesRead = ::read(g_signalSockets[1], buffer, sizeof(buffer));
            (void)bytesRead;
            app.quit();
        });
        std::signal(SIGINT, handleUnixSignal);
        std::signal(SIGTERM, handleUnixSignal);
    }

    QLockFile lockFile(QStringLiteral("/tmp/transTags_linux.lock"));
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(100)) {
        QMessageBox::information(nullptr, QObject::tr("transTags Linux"), QObject::tr("transTags Linux 已在运行中。"));
        return 0;
    }

    QSettings settings;
    WindowController controller;
    controller.setTransparencyStep(settings.value(QStringLiteral("transparencyStep"), 10).toInt());
    controller.setClickThroughEnabled(settings.value(QStringLiteral("clickThrough"), true).toBool());
    controller.setPinEnabled(settings.value(QStringLiteral("pin"), true).toBool());

    SettingsDialog dialog(settings);
    dialog.setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows"), createFallbackIcon()));
    dialog.setStatus(controller.available() ? QObject::tr("已启动") : controller.errorString());
    ToastPopup toast;

    QObject::connect(&dialog, &SettingsDialog::transparencyStepChanged, &controller, &WindowController::setTransparencyStep);
    QObject::connect(&dialog, &SettingsDialog::clickThroughChanged, &controller, &WindowController::setClickThroughEnabled);
    QObject::connect(&dialog, &SettingsDialog::pinChanged, &controller, &WindowController::setPinEnabled);
    QObject::connect(&dialog, &SettingsDialog::toggleClickThroughRequested, &controller, &WindowController::toggleClickThrough);
    QObject::connect(&dialog, &SettingsDialog::unlockAllPinnedRequested, &controller, &WindowController::unlockAllPinned);
    QObject::connect(&controller, &WindowController::statusChanged, &dialog, &SettingsDialog::setStatus);
    QObject::connect(&controller, &WindowController::statusChanged, &toast, &ToastPopup::showMessage);

    HotkeyThread hotkeys;
    QObject::connect(&hotkeys, &HotkeyThread::transparencyUp, &controller, &WindowController::adjustTransparencyUp);
    QObject::connect(&hotkeys, &HotkeyThread::transparencyDown, &controller, &WindowController::adjustTransparencyDown);
    QObject::connect(&hotkeys, &HotkeyThread::toggleClickThrough, &controller, &WindowController::toggleClickThrough);
    QObject::connect(&hotkeys, &HotkeyThread::togglePin, &controller, &WindowController::togglePin);
    QObject::connect(&hotkeys, &HotkeyThread::centerWindow, &controller, &WindowController::centerWindow);
    QObject::connect(&hotkeys, &HotkeyThread::statusChanged, &dialog, &SettingsDialog::setStatus);
    QObject::connect(&hotkeys, &HotkeyThread::statusChanged, &toast, &ToastPopup::showMessage);

    if (controller.available())
        hotkeys.start();

    QSystemTrayIcon tray;
    tray.setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows"), createFallbackIcon()));
    tray.setToolTip(QObject::tr("transTags Linux"));

    QMenu trayMenu;
    QAction *settingsAction = trayMenu.addAction(QObject::tr("设置"));
    QAction *unlockAction = trayMenu.addAction(QObject::tr("切换鼠标穿透"));
    QAction *unpinAction = trayMenu.addAction(QObject::tr("取消全部置顶"));
    trayMenu.addSeparator();
    QAction *quitAction = trayMenu.addAction(QObject::tr("退出"));

    QObject::connect(settingsAction, &QAction::triggered, [&dialog]() {
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
    });
    QObject::connect(unlockAction, &QAction::triggered, &controller, &WindowController::toggleClickThrough);
    QObject::connect(unpinAction, &QAction::triggered, &controller, &WindowController::unlockAllPinned);
    QObject::connect(quitAction, &QAction::triggered, &app, &QCoreApplication::quit);
    QObject::connect(&tray, &QSystemTrayIcon::activated, [&dialog](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            dialog.show();
            dialog.raise();
            dialog.activateWindow();
        }
    });

    tray.setContextMenu(&trayMenu);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray.show();
        tray.showMessage(QObject::tr("transTags Linux"),
                         QObject::tr("已启动。Alt+←/→ 调透明，Alt+↑ 切换穿透，Alt+↓ 置顶。"),
                         QSystemTrayIcon::Information, 3500);
        toast.showMessage(QObject::tr("transTags Linux 已启动"));
    } else {
        dialog.show();
    }

    if (sessionType() != QStringLiteral("x11")) {
        QTimer::singleShot(600, [&dialog]() {
            dialog.show();
            dialog.raise();
            dialog.activateWindow();
        });
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        hotkeys.requestInterruption();
        hotkeys.wait(1000);
        controller.cleanup();
    });

    return QApplication::exec();
}

#ifdef Bool
#undef Bool
#endif

#include "main.moc"
