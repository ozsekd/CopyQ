/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QClipboard>
#include <QMainWindow>
#include <QMap>
#include <QModelIndex>
#include <QPointer>
#include <QSharedPointer>
#include <QSystemTrayIcon>

class AboutDialog;
class Action;
class ActionDialog;
class ClipboardBrowser;
class ClipboardItem;
class QAction;
class QMimeData;
class TrayMenu;
struct ClipboardBrowserShared;
struct Command;

namespace Ui
{
    class MainWindow;
}

/**
 * Application's main window.
 *
 * Contains search bar and tab widget.
 * Each tab contains one clipboard browser widget.
 *
 * It operates in two modes:
 *  * browse mode with search bar hidden and empty (default) and
 *  * search mode with search bar shown and not empty.
 *
 * If user starts typing text the search mode will become active and
 * the search bar focused.
 * If the text is deleted or escape pressed the browse mode will become active.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = NULL);
        ~MainWindow();

        /** Return true if in browse mode. */
        bool browseMode() const { return m_browsemode; }

        /** Save settings, items in browsers and window geometry. */
        void saveSettings();

        /** Create new action dialog. */
        ActionDialog *createActionDialog();

        bool isTrayMenuVisible() const;

        void setSessionName(const QString &sessionName);

    signals:
        /** Request clipboard change. */
        void changeClipboard(const ClipboardItem *item);

    protected:
        void keyPressEvent(QKeyEvent *event);
        void keyReleaseEvent(QKeyEvent *event);
        void dragEnterEvent(QDragEnterEvent *event);
        void dropEvent(QDropEvent *event);
        bool event(QEvent *event);
        bool eventFilter(QObject *obj, QEvent *event);

        /** Hide (minimize to tray) window on close. */
        void closeEvent(QCloseEvent *event);

        void resizeEvent(QResizeEvent *event);
        void moveEvent(QMoveEvent *event);

#if QT_VERSION < 0x050000
#   ifdef COPYQ_WS_X11
        bool x11Event(XEvent *event);
#   elif defined(Q_OS_WIN)
        bool winEvent(MSG *message, long *result);
#   elif defined(Q_OS_MAC)
        bool macEvent(EventHandlerCallRef caller, EventRef event);
#   endif
#else
        bool nativeEvent(const QByteArray &eventType, void *message, long *result);
#endif

    public slots:
        /**
         * Return browser widget in given tab @a index (or current tab).
         * Load items if not loaded yet.
         */
        ClipboardBrowser *browser(int index = -1);

        /**
         * Find tab with given @a name.
         * Load items if not loaded yet.
         * @return found tab or NULL
         */
        ClipboardBrowser *findTab(const QString &name);

        /**
         * Find tab with given @a name.
         * @return found tab index or -1
         */
        int findTabIndex(const QString &name);

        /** Return tab index for browser widget (-1 if not found). */
        int tabIndex(const ClipboardBrowser *c) const;

        /**
         * Create tab with given @a name if it doesn't exist.
         * @return Existing or new tab with given @a name.
         */
        ClipboardBrowser *createTab(
                const QString &name, //!< Name of the new tab.
                bool save
                //!< If true saveSettings() is called if tab is created.
                );

        /** Return window ID. */
        WId mainWinId() const;

        /** Return window ID of tray menu. */
        WId trayMenuWinId() const;

        /**
         * Show/hide tray menu. Return true only if menu is shown.
         */
        bool toggleMenu();

        /** Return tab names. */
        QStringList tabs() const;

        /** Switch between browse and search mode. */
        void enterBrowseMode(bool browsemode = true);

        /** Show tray popup message. */
        void showMessage(
                const QString &title, //!< Message title.
                const QString &msg, //!< Message text.
                QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                //!< Type of popup.
                int msec = 8000 //!< Show interval.
                );

        /** Show error in tray popup message. */
        void showError(const QString &msg);

        /** Show and focus main window. */
        void showWindow();
        /** Show/hide main window. Return true only if window is shown. */
        bool toggleVisible();
        /** Show window and given tab and give focus to the tab. */
        void showBrowser(const ClipboardBrowser *browser);
        /** Show window and given tab and give focus to the tab. */
        void showBrowser(int index);
        /** Enter browse mode and reset search. */
        void resetStatus();

        /** Close main window and exit the application. */
        void exit();
        /** Load settings. */
        void loadSettings();

        /** Open about dialog. */
        void openAboutDialog();

        /** Open dialog with clipboard content. */
        void showClipboardContent();

        /** Open action dialog for given @a row (or current) in current tab. */
        void openActionDialog(int row = -1);
        /** Open action dialog with given input @a text. */
        WId openActionDialog(const QMimeData &data);

        /** Open preferences dialog. */
        void openPreferences();

        /** Execute action. */
        void action(Action *action);

        /** Execute command on given input data. */
        void action(const QMimeData &data, const Command &cmd,
                    const QModelIndex &outputIndex = QModelIndex());

        /** Open tab creation dialog. */
        void newTab(const QString &name = QString());
        /** Open tab group renaming dialog. */
        void renameTabGroup(const QString &name);
        /** Rename current tab to given name (if possible). */
        void renameTabGroup(const QString &newName, const QString &oldName);
        /** Open tab renaming dialog (for given tab index or current tab). */
        void renameTab(int tab = -1);
        /** Rename current tab to given name (if possible). */
        void renameTab(const QString &name, int tabIndex);
        /** Remove all tab in group. */
        void removeTabGroup(const QString &name);
        /** Remove tab. */
        void removeTab(
                bool ask = true, //!< Ask before removing.
                int tab_index = -1 //!< Tab index or current tab.
                );
        /**
         * Add tab with given name if doesn't exist and focus the tab.
         * @return New or existing tab with given name.
         */
        ClipboardBrowser *addTab(const QString &name);

        /** Create new item in current tab. */
        void editNewItem();
        /** Paste items to current tab. */
        void pasteItems();
        /** Copy selected items in current tab. */
        void copyItems();

        /**
         * Save all items in tab to file.
         * @return True only if all items were successfully saved.
         */
        bool saveTab(
                const QString &fileName,
                int tab_index = -1 //!< Tab index or current tab.
                );
        /** Save all items in tab. Show file dialog. */
        bool saveTab(
                int tab_index = -1 //!< Tab index or current tab.
                );
        /**
         * Load saved items to new tab.
         * @return True only if all items were successfully loaded.
         */
        bool loadTab(const QString &fileName);
        /**
         * Load saved items to new tab.
         * Show file dialog and focus the new tab.
         */
        bool loadTab();

        /** Sort selected items. */
        void sortSelectedItems();
        /** Reverse order of selected items. */
        void reverseSelectedItems();

        /** Add @a data to tab with given name (create if tab doesn't exist). */
        void addToTab(
                const QMimeData *data,
                //!< Item data (it may be updated if item with same text exists).
                const QString &tabName = QString(),
                //!< Tab name of target tab (first tab if empty).
                bool moveExistingToTop = false
                //!< If item already exists, move it to top and select it.
                );

        /** Set next or first tab as current. */
        void nextTab();
        /** Set previous or last tab as current. */
        void previousTab();

        /** Called after clipboard content changes. */
        void clipboardChanged(const ClipboardItem *item);

        /** Set clipboard. */
        void setClipboard(const ClipboardItem *item);

        /** Activate current item. */
        void activateCurrentItem();

        /** Temporarily disable monitoring (i.e. adding new clipboard content to the first tab). */
        void disableMonitoring(bool disable);

        /** Toggle monitoring (i.e. adding new clipboard content to the first tab). */
        void toggleMonitoring();

        /** Return clipboard data. If MIME type is "?" return list of available MIME types. */
        QByteArray getClipboardData(const QString &mime,
                                    QClipboard::Mode mode = QClipboard::Clipboard);

        /** Paste clipboard content to current window. */
        void pasteToCurrentWindow();

    private slots:
        ClipboardBrowser *getTabForTrayMenu();
        void updateTrayMenuItems();
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void onTrayActionTriggered(uint clipboardItemHash);
        void enterSearchMode(const QString &txt);
        void tabChanged(int current);
        void tabMoved(int from, int to);
        void tabMoved(const QString &oldPrefix, const QString &newPrefix, const QString &afterPrefix);
        void tabMenuRequested(const QPoint &pos, int tab);
        void tabMenuRequested(const QPoint &pos, const QString &groupPath);
        void tabCloseRequested(int tab);
        void addItems(const QStringList &items, const QString &tabName);
        void addItems(const QStringList &items, const QModelIndex &index);
        void addItem(const QByteArray &data, const QString &format, const QString &tabName);
        void addItem(const QByteArray &data, const QString &format, const QModelIndex &index);
        void onTimerSearch();

        void actionStarted(Action *action);
        void actionFinished(Action *action);
        void actionError(Action *action);

        void onChangeClipboardRequest(const ClipboardItem *item);

        /** Update WId for paste and last focused window if needed. */
        void updateFocusWindows();

    private:
        /** Create menu bar and tray menu with items. Called once. */
        void createMenu();

        /** Create context menu for @a tab. It will be automatically deleted after closed. */
        void popupTabBarMenu(const QPoint &pos, const QString &tab);

        /** Delete finished action and its menu item. */
        void closeAction(Action *action);

        /** Update tray and window icon depending on current state. */
        void updateIcon();

        void updateWindowTransparency(bool mouseOver = false);

        /** Update name and icon of "disable/enable monitoring" menu actions. */
        void updateMonitoringActions();

        /** Return browser containing item or NULL. */
        ClipboardBrowser *findBrowser(const QModelIndex &index);

        /** Return browser with given ID. */
        ClipboardBrowser *findBrowser(const QString &id);

        /** Return browser index with given ID. */
        int findBrowserIndex(const QString &id);

        /** Return browser widget in given tab @a index (or current tab). */
        ClipboardBrowser *getBrowser(int index = -1) const;

        /** Return true only if main window owns window/widget with given WId. */
        bool isForeignWindow(WId wid);

        /** Call updateFocusWindows() after a small delay. */
        void delayedUpdateFocusWindows();

        /** Show/hide tab bar. **/
        void setHideTabs(bool hide);

        /** Show/hide menu bar. **/
        void setHideMenuBar(bool hide);

        /**
         * Update auto-saving of tab.
         * Don't save the first tab if option "clear_first_tab" is set.
         */
        void updateTabsAutoSaving();

        Ui::MainWindow *ui;
        AboutDialog *aboutDialog;
        QMenu *itemCmdMenu;
        QMenu *cmdMenu;
        QMenu *itemMenu;
        TrayMenu *trayMenu;
        QSystemTrayIcon *tray;
        bool m_browsemode;
        bool m_confirmExit;
        bool m_trayCommands;
        bool m_trayCurrentTab;
        QString m_trayTabName;
        int m_trayItems;
        bool m_trayImages;
        int m_itemPopupInterval;
        int m_lastTab;
        QTimer *m_timerSearch;

        int m_transparency;
        int m_transparencyFocused;

        bool m_hideTabs;
        bool m_hideMenuBar;

        bool m_activateCloses;
        bool m_activateFocuses;
        bool m_activatePastes;

        bool m_monitoringDisabled;
        QPointer<QAction> m_actionToggleMonitoring;
        QPointer<QAction> m_actionMonitoringDisabled;

        bool m_clearFirstTab;

        QMap<Action*, QAction*> m_actions;

        QSharedPointer<ClipboardBrowserShared> m_sharedData;

        bool m_trayItemPaste;
        WId m_trayPasteWindow;
        WId m_pasteWindow;
        WId m_lastWindow;
        QTimer *m_timerUpdateFocusWindows;

        QTimer *m_timerGeometry;

        QString m_sessionName;
    };

#endif // MAINWINDOW_H
