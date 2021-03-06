/****************************************************************************
**
**      Created using Monkey Studio IDE v1.8.4.0 (1.8.4.0)
** Authors   : Filipe Azevedo aka Nox P@sNox <pasnox@gmail.com>
** Project   : Fresh Library
** FileName  : pPartitionModel_linux.cpp
** Date      : 2011-02-20T00:41:35
** License   : LGPL v3
** Home Page : http://bettercodes.org/projects/fresh
** Comment   : Fresh Library is a Qt 4 extension library providing set of new core & gui classes.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU Leser General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This package is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program. If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/
#include "pPartitionModel.h"

//http://svn.openpilot.org/browse/OpenPilot/trunk/ground/openpilotgcs/src/plugins/rawhid/usbmonitor.h?r=3021

#include <libudev.h>
#include <sys/statfs.h>

#include <QStringList>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDebug>

class DisksSession : public QObject
{
    Q_OBJECT
    
public:
    DisksSession( pPartitionModel* model )
        : QObject( model )
    {
        mModel = model;
        mContext = udev_new();
        mMonitor = udev_monitor_new_from_netlink( mContext, "udev" );
        
        if ( !mContext ) {
            qWarning( "%s: UDev not valid.", Q_FUNC_INFO );
            return;
        }
        
        if ( !mMonitor ) {
            qWarning( "%s: UDevMonitor not valid.", Q_FUNC_INFO );
            return;
        }
        
        udev_monitor_filter_add_match_subsystem_devtype( mMonitor, "block", NULL );
        udev_monitor_enable_receiving( mMonitor );
        
        mNotifier = new QSocketNotifier( udev_monitor_get_fd( mMonitor ), QSocketNotifier::Read, this );
        
        connect( mNotifier, SIGNAL( activated( int ) ), this, SLOT( deviceEventReceived() ) );

        start();
    }
    
    virtual ~DisksSession()
    {
        udev_monitor_unref( mMonitor );
        udev_unref( mContext );
    }
    
    pPartition createPartition( struct udev_device* device )
    {
        struct udev_list_entry* entries = udev_device_get_properties_list_entry( device );
        struct udev_list_entry* entry = 0;
        pPartition partition;
        QVariantMap properties;
        
        udev_list_entry_foreach( entry, entries ) {
            const QString name = udev_list_entry_get_name( entry );
            const QString value = QString::fromLocal8Bit( udev_device_get_property_value( device, name.toLocal8Bit().constData() ) );

            properties[ name ] = value;
        }
        
        if ( properties.contains( "UDISKS_PARTITION_TYPE" ) ) {
            properties[ "UDISKS_PARTITION_TYPE" ] = properties[ "UDISKS_PARTITION_TYPE" ].toString().toLongLong( 0, 16 );
        }
        
        const QString devName = QFileInfo( properties[ "DEVNAME" ].toString() ).fileName();
        QDBusMessage question = QDBusMessage::createMethodCall( "org.freedesktop.UDisks", QString( "/org/freedesktop/UDisks/devices/%1" ).arg( devName ), "org.freedesktop.DBus.Properties", "Get" );
        question << "org.freedesktop.UDisks.Device" << "DeviceMountPaths";
        QDBusMessage answer = QDBusConnection::systemBus().call( question, QDBus::Block, 1 );
        QStringList mountPoints;
        
        foreach ( const QVariant& variant, answer.arguments() ) {
            const QStringList values = variant.value<QDBusVariant>().variant().toStringList();
            
            if ( !values.isEmpty() ) {
                mountPoints << values;
            }
        }
        
        if ( pPartition::isWBFSPartition( properties[ "DEVNAME" ].toString() ) ) {
            properties[ "UDISKS_PARTITION_TYPE" ] = 0x25;
            properties[ "ID_FS_TYPE" ] = pPartition::fileSystemIdToString( 0x25 );
        }
        
        qint64 total = properties.value( "UDISKS_PARTITION_SIZE", -1 ).toLongLong();
        qint64 free = -1;
        
        if ( !mountPoints.isEmpty() ) {
            struct statfs stats;
            
            if ( statfs( qPrintable( mountPoints.first() ), &stats ) == 0 ) {
                /*const qint64 total = stats.f_blocks *stats.f_bsize;
                partition.used = total -( stats.f_bfree *stats.f_bsize );
                partition.free = partition.total -partition.used;*/
                
                total = stats.f_blocks *stats.f_bsize;
                free = stats.f_bfree *stats.f_bsize;
            }
            
            properties[ "UDISKS_PARTITION_MOUNT_POINTS" ] = mountPoints;
        }
        
        properties[ "REMOVABLE" ] = "0";

        // Check removable attr
        struct udev_device* parent = 0;
        
        if ( properties[ "DEVTYPE" ] == "partition" ) {
            parent = udev_device_get_parent( device );
        }
        else if ( properties[ "DEVTYPE" ] == "disk" ) {
            parent = device; // disk without partitions
        }

        if ( parent != 0 ) {
            const char* value = udev_device_get_sysattr_value( parent, "removable" );

            if ( value ) {
                properties[ "REMOVABLE" ] = QString::fromLocal8Bit( value );
            }
        }
        
        partition.setProperties( properties );
        partition.updateSizes( total, free );
        
        return partition;
    }
    
protected:
    void start()
    {
        struct udev_enumerate* enumerate  = udev_enumerate_new( mContext );
        
        if ( !enumerate ) {
            qWarning( "%s: Can't enumerate.", Q_FUNC_INFO );
            return;
        }
        
        udev_enumerate_add_match_subsystem( enumerate, "block" );
        udev_enumerate_scan_devices( enumerate );
        
        struct udev_device* device = 0;
        struct udev_list_entry* entries = udev_enumerate_get_list_entry( enumerate );
        struct udev_list_entry* entry = 0;
        
        udev_list_entry_foreach( entry, entries ) {
            device = udev_device_new_from_syspath( mContext, udev_list_entry_get_name( entry ) );
            
            if ( !device ) {
                continue;
            }
            
            diskAppeared( device );
            
            udev_device_unref( device );
        }

        udev_enumerate_unref( enumerate );
    }

protected slots:
    void deviceEventReceived()
    {
        struct udev_device* device = udev_monitor_receive_device( mMonitor );
        
        if ( device ) {
            const QString action = QString( udev_device_get_action( device ) );
            const QString devtype = QString( udev_device_get_devtype( device ) );
            
            if ( action == "add" ) {
                diskAppeared( device );
            }
            else if ( action == "change" ) {
                diskChanged( device );
            }
            else if ( action == "remove" ){
                diskDisappeared( device );
            }
            else {
                qWarning() << Q_FUNC_INFO << "------- Got Device Event" <<  "type" << devtype << " - action" << action;
            }

            udev_device_unref( device );
        }
        else {
            qWarning( "%s: No Device. An error occured.", Q_FUNC_INFO );
        }
    }
    
    void diskAppeared( struct udev_device* device )
    {
        const pPartition partition = createPartition( device );
        
        if ( partition.properties()[ "DEVTYPE" ] == "partition"
            || ( partition.properties()[ "ID_TYPE" ] == "cd" 
                && partition.properties()[ "ID_FS_USAGE" ] == "filesystem" ) ) {
            mModel->updatePartition( partition );
        }
    }
    
    void diskChanged( struct udev_device* device )
    {
        diskAppeared( device );
    }
    
    void diskDisappeared( struct udev_device* device )
    {
        const QString devicePath = QString::fromLocal8Bit( udev_device_get_property_value( device, "DEVNAME" ) );
        mModel->removePartition( devicePath );
    }
    
protected:
    pPartitionModel* mModel;
    udev* mContext;
    udev_monitor* mMonitor;
    QSocketNotifier* mNotifier;
};

void pPartitionModel::platformInit()
{
    emit layoutAboutToBeChanged();
    mData = new DisksSession( this );
    emit layoutChanged();
}

void pPartitionModel::platformDeInit()
{
    delete (DisksSession*)mData;
}

void pPartitionModel::platformUpdate()
{
    // Code commented as the linux implementation has the concept of live auto update, thanks to UDev librarie :)
    /*const QStringList partitions = customPartitions();
    
    emit layoutAboutToBeChanged();
    delete (DisksSession*)mData;
    mPartitions.clear();
    foreach ( const QString& partition, partitions ) {
        mPartitions << pPartition( partition );
    }
    mData = new DisksSession( this );
    emit layoutChanged();*/
}

#include "pPartitionModel_linux.moc"
