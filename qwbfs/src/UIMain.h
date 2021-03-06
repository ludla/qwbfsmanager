/****************************************************************************
**
**      Created using Monkey Studio IDE v1.8.4.0 (1.8.4.0)
** Authors   : Filipe Azevedo aka Nox P@sNox <pasnox@gmail.com>
** Project   : QWBFS Manager
** FileName  : UIMain.h
** Date      : 2010-06-16T14:19:29
** License   : GPL2
** Home Page : https://github.com/pasnox/qwbfsmanager
** Comment   : QWBFS Manager is a cross platform WBFS manager developed using C++/Qt4.
** It's currently working fine under Windows (XP to Seven, 32 & 64Bits), Mac OS X (10.4.x to 10.6.x), Linux & unix like.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This package is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program. If not, see <http://www.gnu.org/licenses/>.
**
** In addition, as a special exception, the copyright holders give permission
** to link this program with the OpenSSL project's "OpenSSL" library (or with
** modified versions of it that use the same license as the "OpenSSL"
** library), and distribute the linked executables. You must obey the GNU
** General Public License in all respects for all of the code used other than
** "OpenSSL".  If you modify file(s), you may extend this exception to your
** version of the file(s), but you are not obligated to do so. If you do not
** wish to do so, delete this exception statement from your version.
**
****************************************************************************/
#ifndef UIMAIN_H
#define UIMAIN_H

#include "ui_UIMain.h"

class QMenuBar;
class QFileSystemModel;
class QNetworkReply;
class pNetworkAccessManager;
class pPaypalButton;
class pUpdateChecker;

namespace QWBFS {
namespace Model {
    class DiscModel;
}; // Model
}; // QWBFS

class UIMain : public QMainWindow, public Ui::UIMain
{
    Q_OBJECT

public:
    UIMain( QWidget* parent = 0 );
    virtual ~UIMain();
    
    virtual bool event( QEvent* event );
    
    pNetworkAccessManager* cache() const;
    pQueuedMessageToolBar* messageToolBar() const;

protected:
#if defined( Q_OS_MAC )
    QMenuBar* mMenuBar;
#endif
    QMenu* mActions;
    pPaypalButton* mDonationWidget;
    QFileSystemModel* mFoldersModel;
    QFileSystemModel* mFilesModel;
    pNetworkAccessManager* mCache;
    QString mLastDiscId;
    pUpdateChecker* mUpdateChecker;
    
    virtual void showEvent( QShowEvent* event );
    virtual void closeEvent( QCloseEvent* event );
    virtual bool eventFilter( QObject* object, QEvent* event );
    
    void connectView( PartitionWidget* widget );

protected slots:
    void localeChanged();
    void loadProperties( bool firstInit = true );
    void saveProperties();
    void changeLocaleRequested();
    void propertiesChanged();
    void openViewRequested();
    void closeViewRequested();
    void coverRequested( const QString& id );
    void progress_jobFinished( const QWBFS::Model::Disc& disc );
    void networkAccessManager_finished( QNetworkReply* reply );
    void networkAccessManager_cached( const QUrl& url );
    void networkAccessManager_error( const QUrl& url, const QString& message );
    void networkAccessManager_cacheCleared();
    void on_aReloadPartitions_triggered();
    void on_aQuit_triggered();
    void on_aAbout_triggered();
    void on_aProperties_triggered();
    void on_aConvertToWBFSFiles_triggered();
    void on_aConvertToISOFiles_triggered();
    void on_aRenameDiscsInFolder_triggered();
    void on_tvFolders_activated( const QModelIndex& index );
    void on_tbReloadDrives_clicked();
    void on_cbDrives_currentIndexChanged( const QString& text );
    void on_tbClearExport_clicked();
    void on_tbRemoveExport_clicked();
    void on_tbExport_clicked();
};

#endif // UIMAIN_H
