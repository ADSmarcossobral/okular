/***************************************************************************
 *   Copyright (C) 2002 by Wilco Greven <greven@kde.org>                   *
 *   Copyright (C) 2003-2004 by Christophe Devriese                        *
 *                         <Christophe.Devriese@student.kuleuven.ac.be>    *
 *   Copyright (C) 2003 by Andy Goossens <andygoossens@telenet.be>         *
 *   Copyright (C) 2003 by Laurent Montel <montel@kde.org>                 *
 *   Copyright (C) 2004 by Dominique Devriese <devriese@kde.org>           *
 *   Copyright (C) 2004-2007 by Albert Astals Cid <aacid@kde.org>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef _PART_H_
#define _PART_H_

#include <kparts/part.h>
#include <qicon.h>
#include <qlist.h>
#include <qpointer.h>
#include <qprocess.h>
#include "core/observer.h"
#include "core/document.h"
#include "kdocumentviewer.h"

#include <QtDBus/QtDBus>

class QAction;
class QWidget;
class QPrinter;

class KUrl;
class KConfigGroup;
class KDirWatch;
class KToggleAction;
class KToggleFullScreenAction;
class KSelectAction;
class KAboutData;
class KTemporaryFile;

class FindBar;
class ThumbnailList;
class PageSizeLabel;
class PageView;
class PageViewTopMessage;
class PresentationWidget;
class ProgressWidget;
class SearchWidget;
class Sidebar;
class TOC;
class MiniBar;

namespace Okular
{
class ExportFormat;
}

class BrowserExtension;

/**
 * This is a "Part".  It that does all the real work in a KPart
 * application.
 *
 * @short Main Part
 * @author Wilco Greven <greven@kde.org>
 * @version 0.2
 */
class Part : public KParts::ReadOnlyPart, public Okular::DocumentObserver, public KDocumentViewer
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.okular")
    Q_INTERFACES(KDocumentViewer)

    public:
        // Default constructor
        Part(QWidget* parentWidget, QObject* parent, const QVariantList& args);

        // Destructor
        ~Part();

        // inherited from DocumentObserver
        uint observerId() const { return PART_ID; }
        void notifySetup( const QVector< Okular::Page * > &pages, int setupFlags );
        void notifyViewportChanged( bool smoothMove );
        void notifyPageChanged( int page, int flags );

        bool openDocument(const KUrl& url, uint page);
        void startPresentation();
        QStringList supportedMimeTypes() const;

        KUrl realUrl() const;

    public slots:                // dbus
        Q_SCRIPTABLE Q_NOREPLY void goToPage(uint page);
        Q_SCRIPTABLE Q_NOREPLY void openDocument( const QString &doc );
        Q_SCRIPTABLE uint pages();
        Q_SCRIPTABLE uint currentPage();
        Q_SCRIPTABLE QString currentDocument();
        Q_SCRIPTABLE void slotPreferences();
        Q_SCRIPTABLE void slotFind();
        Q_SCRIPTABLE void slotPrintPreview();
        Q_SCRIPTABLE void slotPreviousPage();
        Q_SCRIPTABLE void slotNextPage();
        Q_SCRIPTABLE void slotGotoFirst();
        Q_SCRIPTABLE void slotGotoLast();
        Q_SCRIPTABLE void slotTogglePresentation();

    signals:
        void enablePrintAction(bool enable);

    protected:
        // reimplemented from KParts::ReadOnlyPart
        bool openFile();
        bool openUrl(const KUrl &url);
        bool closeUrl();

    protected slots:
        // connected to actions
        void openUrlFromDocument(const KUrl &url);
        void openUrlFromBookmarks(const KUrl &url);
        void slotGoToPage();
        void slotHistoryBack();
        void slotHistoryNext();
        void slotAddBookmark();
        void slotPreviousBookmark();
        void slotNextBookmark();
        void slotFindNext();
        void slotSaveFileAs();
        void slotSaveCopyAs();
        void slotGetNewStuff();
        void slotNewConfig();
        void slotNewGeneratorConfig();
        void slotShowMenu(const Okular::Page *page, const QPoint &point);
        void slotShowProperties();
        void slotShowEmbeddedFiles();
        void slotShowLeftPanel();
        void slotShowPresentation();
        void slotHidePresentation();
        void slotExportAs(QAction *);
        bool slotImportPSFile();
        void slotAboutBackend();
        void slotReload();
        void close();
        void cannotQuit();
        void slotShowFindBar();
        void slotHideFindBar();
        void setMimeTypes(KIO::Job *job);
        void loadCancelled(const QString &reason);
        void setWindowTitleFromDocument();
        // can be connected to widget elements
        void updateViewActions();
        void updateBookmarksActions();
        void enableTOC(bool enable);
        void slotRebuildBookmarkMenu();

    public slots:
        // connected to Shell action (and browserExtension), not local one
        void slotPrint();
        void restoreDocument(const KConfigGroup &group);
        void saveDocumentRestoreInfo(KConfigGroup &group);
        void slotFileDirty( const QString& );
        void slotDoFileDirty();
        void psTransformEnded(int, QProcess::ExitStatus);

    private:
        void setupPrint( QPrinter &printer );
        void doPrint( QPrinter &printer );
        bool handleCompressed( QString &destpath, const QString &path, const QString &compressedMimetype );
        void rebuildBookmarkMenu( bool unplugActions = true );
        void updateAboutBackendAction();
        void unsetDummyMode();

        KTemporaryFile *m_tempfile;

        // the document
        Okular::Document * m_document;
        QString m_temporaryLocalFile;

        // main widgets
        Sidebar *m_sidebar;
        SearchWidget *m_searchWidget;
        FindBar * m_findBar;
        PageViewTopMessage * m_topMessage;
        PageViewTopMessage * m_formsMessage;
        QPointer<ThumbnailList> m_thumbnailList;
        QPointer<PageView> m_pageView;
        QPointer<TOC> m_toc;
        QPointer<MiniBar> m_miniBar;
        QPointer<PresentationWidget> m_presentationWidget;
        QPointer<ProgressWidget> m_progressWidget;
        QPointer<PageSizeLabel> m_pageSizeLabel;

        // document watcher (and reloader) variables
        KDirWatch *m_watcher;
        QTimer *m_dirtyHandler;
        Okular::DocumentViewport m_viewportDirty;
        bool m_wasPresentationOpen;
        int m_dirtyToolboxIndex;
        bool m_wasSidebarVisible;
        bool m_fileWasRemoved;

        // Remember the search history
        QStringList m_searchHistory;

        // actions
        QAction *m_gotoPage;
        QAction *m_prevPage;
        QAction *m_nextPage;
        QAction *m_firstPage;
        QAction *m_lastPage;
        QAction *m_historyBack;
        QAction *m_historyNext;
        QAction *m_addBookmark;
        QAction *m_prevBookmark;
        QAction *m_nextBookmark;
        QAction *m_copy;
        QAction *m_selectAll;
        QAction *m_find;
        QAction *m_findNext;
        QAction *m_saveAs;
        QAction *m_saveCopyAs;
        QAction *m_printPreview;
        QAction *m_showProperties;
        QAction *m_showEmbeddedFiles;
        QAction *m_exportAs;
        QAction *m_exportAsText;
        QAction *m_showPresentation;
        KToggleAction* m_showMenuBarAction;
        KToggleAction* m_showLeftPanel;
        KToggleFullScreenAction* m_showFullScreenAction;
        QAction *m_aboutBackend;
        QAction *m_reload;

        bool m_actionsSearched;
        BrowserExtension *m_bExtension;

        QList<Okular::ExportFormat> m_exportFormats;
        QList<QAction*> m_bookmarkActions;
        bool m_cliPresentation;
        QString m_addBookmarkText;
        QIcon m_addBookmarkIcon;

        bool m_dummyMode;

        KUrl m_realUrl;

        KXMLGUIClient *m_generatorGuiClient;

    private slots:
        void slotGeneratorPreferences();
};

#endif

/* kate: replace-tabs on; indent-width 4; */
