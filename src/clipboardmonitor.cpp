#include "clipboardmonitor.h"
#include "clipboardserver.h"
#include "configurationmanager.h"
#include "clipboarditem.h"
#include <QMimeData>

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

ClipboardMonitor::ClipboardMonitor(int &argc, char **argv) :
    App(argc, argv), m_newdata(NULL), m_ignore(false), m_lastClipboard(0)
#ifdef Q_WS_X11
  , m_lastSelection(0), m_dsp(NULL)
#endif
{
    m_socket = new QLocalSocket(this);
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
    connect( m_socket, SIGNAL(disconnected()),
             this, SLOT(quit()) );
    m_socket->connectToServer( ClipboardServer::monitorServerName() );
    if ( !m_socket->waitForConnected(2000) )
        exit(1);

    ConfigurationManager *cm = ConfigurationManager::instance();
    setFormats( cm->value(ConfigurationManager::Formats).toString() );
    setCheckClipboard( cm->value(ConfigurationManager::CheckClipboard).toBool() );

#ifdef Q_WS_X11
    setCopyClipboard( cm->value(ConfigurationManager::CopyClipboard).toBool() );
    setCheckSelection( cm->value(ConfigurationManager::CheckSelection).toBool() );
    setCopySelection( cm->value(ConfigurationManager::CopySelection).toBool() );
    m_timer.setSingleShot(true);
    m_timer.setInterval(100);
    connect(&m_timer, SIGNAL(timeout()),
            this, SLOT(updateSelection()));
#endif

    m_updatetimer.setSingleShot(true);
    m_updatetimer.setInterval(500);
    connect( &m_updatetimer, SIGNAL(timeout()),
             this, SLOT(updateTimeout()) );

    connect( QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
             this, SLOT(checkClipboard(QClipboard::Mode)) );

#ifdef Q_WS_X11
    checkClipboard(QClipboard::Selection);
#endif
    checkClipboard(QClipboard::Clipboard);
}

ClipboardMonitor::~ClipboardMonitor()
{
#ifdef Q_WS_X11
    if (m_dsp)
        XCloseDisplay(m_dsp);
#endif
}

void ClipboardMonitor::setFormats(const QString &list)
{
    m_formats = list.split( QRegExp("[;, ]+") );
}

uint ClipboardMonitor::hash(const QMimeData &data)
{
    QByteArray bytes;
    foreach( QString mime, m_formats ) {
        bytes = data.data(mime);
        if ( !bytes.isEmpty() )
            break;
    }

    // return 0 when data is empty
    return qHash(bytes);
}

#ifdef Q_WS_X11
bool ClipboardMonitor::updateSelection()
{
    // wait while selection is incomplete, i.e. mouse button or
    // shift key is pressed
    if ( m_timer.isActive() )
        return false;
    if ( m_dsp || (m_dsp = XOpenDisplay(NULL)) ) {
        Window root = DefaultRootWindow(m_dsp);
        XEvent event;

        XQueryPointer(m_dsp, root,
                      &event.xbutton.root, &event.xbutton.window,
                      &event.xbutton.x_root, &event.xbutton.y_root,
                      &event.xbutton.x, &event.xbutton.y,
                      &event.xbutton.state);

        if( event.xbutton.state &
                (Button1Mask | ShiftMask) ) {
            m_timer.start();
            return false;
        }
    }

    checkClipboard(QClipboard::Selection);
    return true;
}

void ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
    QClipboard *clipboard;
    const QMimeData *data;
    uint new_hash;
    uint *hash1 = NULL, *hash2 = NULL;
    bool synchronize;
    QClipboard::Mode mode2;

    if (m_ignore) return;
    m_ignore = true;

    if ( m_checkclip && mode == QClipboard::Clipboard ) {
        mode2 = QClipboard::Selection;
        hash1 = &m_lastClipboard;
        hash2 = &m_lastSelection;
        synchronize = m_copyclip;
    } else if ( m_checksel && mode == QClipboard::Selection &&
                updateSelection() ) {
        mode2 = QClipboard::Clipboard;
        hash1 = &m_lastSelection;
        hash2 = &m_lastClipboard;
        synchronize = m_copysel;
    } else {
        m_ignore = false;
        return;
    }

    clipboard = QApplication::clipboard();
    data = clipboard->mimeData(mode);
    if (data) {
        new_hash = hash(*data);
        // is clipboard data different?
        if ( new_hash != *hash1 ) {
            // synchronize clipboard and selection
            if( synchronize && new_hash != *hash2 ) {
                clipboard->setMimeData( cloneData(*data, &m_formats), mode2 );
                *hash2 = new_hash;
            }
            // notify clipboard server only if text size is more than 1 byte
            if (!data->hasText() || data->text().size() > 1) {
                *hash1 = new_hash;
                clipboardChanged(mode, cloneData(*data, &m_formats));
            }
        }
    }
    m_ignore = false;
}
#else /* !Q_WS_X11 */
void ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
    if (m_ignore) return;

    if ( m_checkclip && mode == QClipboard::Clipboard ) {
        const QMimeData *data;
        QClipboard *clipboard = QApplication::clipboard();
        data = clipboard->mimeData(mode);
        clipboardChanged(mode, cloneData(*data, &m_formats));
    }
}
#endif

void ClipboardMonitor::clipboardChanged(QClipboard::Mode, QMimeData *data)
{
    ClipboardItem item;

    foreach ( QString mime, data->formats() ) {
        item.setData(mime, data->data(mime));
    }

    // send clipboard item
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << item;
    QByteArray zipped = qCompress(bytes);

    QDataStream(m_socket) << (quint32)zipped.length() << zipped;
    m_socket->flush();
}

void ClipboardMonitor::updateTimeout()
{
    if (m_newdata) {
        QMimeData *data = m_newdata;
        m_newdata = NULL;

        updateClipboard(*data, true);

        delete data;
    }
}

void ClipboardMonitor::readyRead()
{
    do {
        QByteArray msg;
        if( !readBytes(m_socket, msg) ) {
            // something is wrong -> exit
            log( tr("Incorrect message received!"), LogError );
            exit(3);
            return;
        }

        QDataStream in2(&msg, QIODevice::ReadOnly);

        ClipboardItem item;
        in2 >> item;

        updateClipboard( *item.data() );
    } while ( m_socket->bytesAvailable() );
}

void ClipboardMonitor::updateClipboard(const QMimeData &data, bool force)
{
    uint h = hash(data);
    if ( h == m_lastClipboard && h == m_lastSelection ) {
        // data already in clipboard
        return;
    }

    if (m_newdata) {
        delete m_newdata;
    }
    m_newdata = cloneData(data);

    if ( !force && m_updatetimer.isActive() ) {
        return;
    }

    QMimeData* newdata2 = cloneData(data);

    QClipboard *clipboard = QApplication::clipboard();
    if ( h != m_lastClipboard ) {
        m_ignore = true;
        clipboard->setMimeData(m_newdata, QClipboard::Clipboard);
        m_ignore = false;
        m_lastClipboard = h;
    } else {
        delete m_newdata;
    }
    if ( h != m_lastSelection ) {
        m_ignore = true;
        clipboard->setMimeData(newdata2, QClipboard::Selection);
        m_ignore = false;
        m_lastSelection = h;
    } else {
        delete newdata2;
    }

    m_newdata = NULL;

    m_updatetimer.start();
}