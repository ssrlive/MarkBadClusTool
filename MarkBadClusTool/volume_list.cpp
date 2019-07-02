//volume_list.cpp for class---CVolumeList
//author:lzc
//date:2012/11/10
//e-mail:hackerlzc@126.com

#include"stdafx.h"
#include<windows.h>
#include<winioctl.h>
#include<assert.h>
#include"layout_mbr.h"
#include"volume_list.h"

//类实现


CVolumeList::CVolumeList(LPSTR DiskPath,DWORD DiskId)
/*++
功能描述：构造函数

参数 
    DiskPath:磁盘设备路径

返回值：无
--*/
:m_VolumeCount(0),
m_DiskId(DiskId)
{
    assert( DiskPath != NULL);

    m_hDisk = CreateFile( DiskPath,
                        GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
    if( m_hDisk == INVALID_HANDLE_VALUE ) {
        DbgPrint("open disk device failed!");
    }
    RtlZeroMemory( m_tbl_VolumeOffset,sizeof(m_tbl_VolumeOffset));
    memset( &m_tbl_VolumeOwnerDiskId,0xcc,sizeof(m_tbl_VolumeOwnerDiskId));
    CHAR volumePath[]="\\\\.\\C:";
    DWORD bytesReturned = 0;
    for( CHAR letter = 'C';letter <='Z';letter++)
    {
        volumePath[4]=letter;
        HANDLE hVolume = CreateFile( volumePath,
                            GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
        if( hVolume == INVALID_HANDLE_VALUE )
            continue;
        
        VOLUME_DISK_EXTENTS     volumeExtents={0};
        BOOL bOk = DeviceIoControl(
          hVolume,
          IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
          NULL,
          0,         
          &volumeExtents,
          sizeof(volumeExtents),
          &bytesReturned,
          NULL
        );
        if( !bOk )
        {
            m_tbl_VolumeOffset[letter - 'A'] = 0;
        }
        else
        {
            assert( volumeExtents.NumberOfDiskExtents == 1);
            m_tbl_VolumeOffset[letter - 'A'] = 
                volumeExtents.Extents[0].StartingOffset.QuadPart;
            m_tbl_VolumeOwnerDiskId[letter - 'A'] = 
                volumeExtents.Extents[0].DiskNumber;
        }

        CloseHandle( hVolume );
    }

    InitVolumeList();
}

CVolumeList::~CVolumeList()
/*++
功能描述：柝构函数，释放本类动态申请的内存资源

参数：无

返回值：无

--*/
{
    ReleaseAllResources();
}

WORD CVolumeList::GetVolumeCount()
/*++
功能描述：磁盘中卷数量

参数：无

返回值：磁盘中卷数量

--*/
{
    return m_VolumeCount;
}

BOOL CVolumeList::GetVolumeByIndex(WORD index, PVOLUME_NODE *result)
/*++
功能描述：通过卷序数搜索相应卷信息结点

参数
    index:卷序号
    result:调用者提供的指针空间，用以存放结果卷信息结点的指针

返回值：
    TRUE:搜索成功，*result指向正确的结点
    FALSE:搜索失败，*result置为NULL
--*/
{
    PLIST_ENTRY pEntry = NULL;

    for( pEntry = m_VolumeListHead.Flink;
        pEntry != (PLIST_ENTRY)&m_VolumeListHead;
        pEntry = pEntry->Flink)
    {
        PVOLUME_NODE    pVolumeNode = (PVOLUME_NODE)
            CONTAINING_RECORD(pEntry,VOLUME_NODE,List);
        if( pVolumeNode->Index == index )
        {
            *result = pVolumeNode;
            break;
        }
    }
    if( pEntry != (PLIST_ENTRY)&m_VolumeListHead )
        return TRUE;
    else
        return FALSE;
}

PVOLUME_NODE CVolumeList::GetFirstVolume()
/*++
功能描述：返回第一个卷信息结点

参数：无

返回值：成功则返回第一个卷信息结点指针
        失败返回NULL
--*/
{
    if( IsListEmpty( &m_VolumeListHead ))
        return NULL;

    return (PVOLUME_NODE)CONTAINING_RECORD( m_VolumeListHead.Flink,
        VOLUME_NODE,List );
}

PVOLUME_NODE CVolumeList::GetNextVolume(PVOLUME_NODE curVolume)
/*++
功能描述：通过当前已有卷信息结点指针，返回下一个卷信息结点指针

参数    curVolume:当前卷信息结点指针

返回值：下一个卷信息结点指针，失败返回NULL（表示已经到达结尾）

--*/
{
    assert( curVolume != NULL );

    if( curVolume->List.Flink == &m_VolumeListHead )
        return NULL;
    return (PVOLUME_NODE)CONTAINING_RECORD( curVolume->List.Flink,
                                            VOLUME_NODE,
                                            List);
}


VOID CVolumeList::UpdateVolumeList()
/*++
功能描述：更新卷结点信息

参数：无

返回值：无

--*/
{
    ReleaseAllResources();
    InitVolumeList();
}

VOID CVolumeList::ReleaseAllResources()
/*++
功能描述：释放类中所有动态申请的内存资源

参数：无

返回值：无

--*/
{
    PLIST_ENTRY entry = NULL;
    for( entry = RemoveHeadList( &m_VolumeListHead );
        entry != NULL;
        entry = RemoveHeadList( &m_VolumeListHead))
    {
        PVOLUME_NODE node = (PVOLUME_NODE)CONTAINING_RECORD(
            entry,
            VOLUME_NODE,
            List);
        if( node->VolumeName != NULL)free( node->VolumeName );
        free( node );
        node = NULL;
        m_VolumeCount--;
    }

    if( m_hDisk != INVALID_HANDLE_VALUE )
        CloseHandle( m_hDisk );

    assert( m_VolumeCount == 0 );
    assert( IsListEmpty( &m_VolumeListHead ));
}

VOID CVolumeList::InitVolumeList()
/*++
功能描述：初始化卷列表

参数：无

返回值：无

--*/
{
    InitializeListHead( &m_VolumeListHead );
    m_VolumeCount = (WORD)SearchMbrVolume( 0 );

}

DWORD CVolumeList::SearchMbrVolume(DWORD BaseSector, DWORD BaseEbrSector /*= 0*/)
/*++
功能描述：搜索磁盘中的卷，建立卷信息链表

参数：
    BaseSector:MBR 或者 EBR所在的绝对扇区号
    BaseEbrSector: 基本EBR所在扇区绝对号

返回值：搜索到的卷数量

注意：*此函数设计为递归调用函数。
      
      *EBR中描述的逻辑驱动器都是以其所在的EBR的扇区号作为基址的。
       EBR中所描述的DOS扩展分区是以基本EBR的扇区号作为基址的
      *基本EBR是指MBR中描述的EBR。
--*/
{
    DWORD   i = 0;
    BOOL    bOk = FALSE;
    MBR_SECTOR  mbrSector = {0};    //同时作为EBR使用  
    DWORD   VolumeCount = 0;

    if( m_hDisk == INVALID_HANDLE_VALUE )
    {
        DbgPrint("Invalid handle value!");
        return 0;
    }

    bOk = ReadSector( m_hDisk,&mbrSector,sizeof( mbrSector),BaseSector);
    if( !bOk )
    {
        DbgPrint("Read sector failed!");
        return 0;
    }

    if( mbrSector.end_flag != 0xaa55 )
    {
        DbgPrint("mbr sector is invalid!");
        return 0;
    }

    for( i = 0;i < 4;i++)
    {
        if( mbrSector.dpt[i].partition_type_indicator == PARTITION_TYPE_ILLEGAL)
            continue;

        if( IsVolumeTypeSupported( mbrSector.dpt[i].partition_type_indicator))
        {
            //支持的卷类型
            PVOLUME_NODE    node = {0};
            CHAR            buffer[256];

            node = (PVOLUME_NODE)malloc( sizeof( VOLUME_NODE ));
            assert( node != NULL);
            node->Index = m_VolumeCount;
            VolumeCount++;
            m_VolumeCount++;
            node->TotalSectors.QuadPart = mbrSector.dpt[i].total_sectors;
            node->StartSector.QuadPart = BaseSector + mbrSector.dpt[i].sectors_precding;
            node->Type = mbrSector.dpt[i].partition_type_indicator;
            sprintf_s( buffer,"Volume%d",m_VolumeCount );
            size_t len = strlen( buffer ) + 1;
            node->VolumeName = (LPSTR)malloc( len );
            assert( node->VolumeName != NULL);
            strcpy_s( node->VolumeName,len,buffer );
            
            if( node->Type == PARTITION_TYPE_NTFS 
                || node->Type == PARTITION_TYPE_NTFS_HIDDEN)
                node->TypeName = "NTFS";
            else if( node->Type == PARTITION_TYPE_FAT32
                || node->Type == PARTITION_TYPE_FAT32_HIDDEN )
                node->TypeName = "FAT32";
            else node->TypeName = "Unknown";

            //搜索分区对应的盘符
            node->VolumeLetter='-';
            for( CHAR letter ='C';letter <='Z';letter++)
            {
                if( m_tbl_VolumeOwnerDiskId[letter-'A'] == m_DiskId &&
                    m_tbl_VolumeOffset[letter-'A'] == node->StartSector.QuadPart*MBR_SECTOR_SIZE)
                {
                    node->VolumeLetter = letter;
                    break;
                }
            }

            InsertTailList( &m_VolumeListHead,&node->List );
            
        }
        else
        {
            //非支持的卷类型（判断是否是扩展分区）
            if( mbrSector.dpt[i].partition_type_indicator ==
                            PARTITION_TYPE_EXTENDED ||
               mbrSector.dpt[i].partition_type_indicator == 
                            PARTITION_TYPE_EXTENDED_SMALL)
            {
                VolumeCount += SearchMbrVolume( mbrSector.dpt[i].sectors_precding + BaseEbrSector,
                    BaseEbrSector>0?BaseEbrSector:mbrSector.dpt[i].sectors_precding);
            }
        }//end if

    }//end for

    return VolumeCount;
}

BOOL CVolumeList::IsVolumeTypeSupported(BYTE type)
/*++
功能描述：判断是否为本软件支持的卷类型

参数：卷类型码

返回值：支持返回TRUE，否则返回FALSE
--*/
{
    if( type == PARTITION_TYPE_NTFS )
        return TRUE;
    else if( type == PARTITION_TYPE_NTFS_HIDDEN )
        return TRUE;
//    else if( type == PARTITION_TYPE_FAT32)
//        return TRUE;
//    else if( type == PARTITION_TYPE_FAT32_HIDDEN )
//        return TRUE;
    else
        return FALSE;
}