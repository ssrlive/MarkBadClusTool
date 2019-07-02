//repair_controller.cpp for class-- CRepairController
//author:lzc
//date:2012/11/14
//e-mail:hackerlzc@126.com

#include "stdafx.h"
#include<windows.h>
#include"repair_controller.h"

//类实现

CRepairController::CRepairController(LPSTR lpszDiskPath,LONGLONG StartSector,LONGLONG NumberOfSectors)
/*++
功能描述：构造函数，初始化类成员变量
--*/
:
m_hDisk( INVALID_HANDLE_VALUE ),
m_lpMessageFunc( NULL)
{
    m_VolumeStartSector.QuadPart = StartSector;
    m_VolumeTotalSectors.QuadPart = NumberOfSectors;
    RtlZeroMemory( &m_BlockInforHead,sizeof( m_BlockInforHead ));
    InitializeListHead( &m_BlockInforHead.BadBlockList );
    InitializeListHead( &m_BlockInforHead.FreeBlockList );
    InitializeListHead( &m_BlockInforHead.UsedBlockList );
    InitializeListHead( &m_BlockInforHead.DeadBlockList );

    assert( lpszDiskPath != NULL );

    m_hDisk = CreateFile( lpszDiskPath,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
    if( m_hDisk == INVALID_HANDLE_VALUE ) {
        DbgPrint("open disk device failed!");
    }
}

CRepairController::~CRepairController()
/*++
功能描述：析构函数，释放资源
--*/
{
    ReleaseResources();
}

VOID CRepairController::ReleaseResources()
/*++
功能描述：释放本类申请的资源

参数：无

返回值：无

注意：为虚函数，仅仅负责本类申请资源的释放
--*/
{
    assert( IsListEmpty( &m_BlockInforHead.BadBlockList));
    assert( IsListEmpty( &m_BlockInforHead.UsedBlockList));
    assert( IsListEmpty( &m_BlockInforHead.FreeBlockList));
    assert( m_BlockInforHead.BadBlockSize.QuadPart == 0 );
    assert( m_BlockInforHead.FreeBlockSize.QuadPart == 0 );
    assert( m_BlockInforHead.UsedBlockSize.QuadPart == 0 );

    if( m_hDisk != INVALID_HANDLE_VALUE )
        CloseHandle( m_hDisk );

    m_VolumeStartSector.QuadPart = 0;
    m_VolumeTotalSectors.QuadPart = 0;
    m_lpMessageFunc = NULL;

    if( !IsListEmpty( &m_BlockInforHead.DeadBlockList))
    {
        //释放链表结点的动态内存空间

        PLIST_ENTRY entry = NULL;
        for( entry = RemoveHeadList( &m_BlockInforHead.DeadBlockList );
            entry != NULL;
            entry = RemoveHeadList( &m_BlockInforHead.DeadBlockList))
        {
            PBLOCK_DESCRIPTOR pBlockDesc = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(
                entry,
                BLOCK_DESCRIPTOR,
                List);
            free( pBlockDesc );
        }
        assert( IsListEmpty( &m_BlockInforHead.DeadBlockList ));
    }
}

PBLOCK_DESCRIPTOR CRepairController::GetFirstUsedBlock()
/*++
功能描述：获取首个已用区块描述信息

参数：无

返回值：区块描述信息指针，不存在则返回NULL

--*/
{
    if( IsListEmpty( &m_BlockInforHead.UsedBlockList))
        return NULL;
    return (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(GetFirstListEntry(&m_BlockInforHead.UsedBlockList),
                                                BLOCK_DESCRIPTOR,
                                                List);
}

PBLOCK_DESCRIPTOR CRepairController::GetNextUsedBlock( PBLOCK_DESCRIPTOR CurrBlock )
/*++
功能描述：获取下一个已用区块描述信息

参数：
    CurrBlock:当前已用区块描述信息指针

返回值：区块描述信息指针，已经到达链表结尾则返回NULL
--*/
{
    if( CurrBlock->List.Flink == &m_BlockInforHead.UsedBlockList )
        return NULL;
    return (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( CurrBlock->List.Flink,
                                                BLOCK_DESCRIPTOR,
                                                List );
}

PBLOCK_DESCRIPTOR CRepairController::GetFirstFreeBlock()
/*++
功能描述：获取首个空闲区块描述信息

参数：无

返回值：区块描述信息指针，不存在则返回NULL

--*/
{
    if( IsListEmpty( &m_BlockInforHead.FreeBlockList))
        return NULL;
    return (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(GetFirstListEntry(&m_BlockInforHead.FreeBlockList),
                                                BLOCK_DESCRIPTOR,
                                                List);
}

PBLOCK_DESCRIPTOR CRepairController::GetNextFreeBlock( PBLOCK_DESCRIPTOR CurrBlock )
/*++
功能描述：获取下一个未用（空闲）区块描述信息

参数：
    CurrBlock:当前未用区块描述信息指针

返回值：区块描述信息指针，已经到达链表结尾则返回NULL

--*/
{
    if( CurrBlock->List.Flink == &m_BlockInforHead.FreeBlockList )
        return NULL;
    return (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(CurrBlock->List.Flink,
                                            BLOCK_DESCRIPTOR,
                                            List );

}

PBLOCK_DESCRIPTOR CRepairController::GetFirstBadBlock()
/*++
功能描述：获取首个坏区块描述信息

参数：无

返回值：区块描述信息指针，不存在则返回NULL

--*/
{
    if( IsListEmpty( &m_BlockInforHead.BadBlockList ))
        return NULL;
    return (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(GetFirstListEntry(&m_BlockInforHead.BadBlockList),
                                                BLOCK_DESCRIPTOR,
                                                List);
}


PBLOCK_DESCRIPTOR CRepairController::GetNextBadBlock( PBLOCK_DESCRIPTOR CurrBlock )
/*++
功能描述：获取下一个坏区块描述信息

参数：
    CurrBlock:当前坏区块描述信息指针

返回值：区块描述信息指针，已经到达链表结尾则返回NULL

--*/
{
    if( CurrBlock->List.Flink == &m_BlockInforHead.BadBlockList )
        return NULL;
    return (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(CurrBlock->List.Flink,
                                                BLOCK_DESCRIPTOR,
                                                List );
}

LONGLONG CRepairController::GetUsedBlockSize()
/*++
功能描述：返回正在使用中的块总容量（以扇区为单位）

参数：无

返回值：使用中的扇区数（通常会对齐到簇的整数倍）
--*/
{
    return m_BlockInforHead.UsedBlockSize.QuadPart;
}

LONGLONG CRepairController::GetFreeBlockSize()
/*++
功能描述：返回空闲块总容量（以扇区为单位）

参数：无

返回值：空闲扇区数（通常会对齐到簇的整数倍）
--*/
{
    return m_BlockInforHead.FreeBlockSize.QuadPart;
}

LONGLONG CRepairController::GetBadBlockSize()
/*++
功能描述：返回坏块总容量（以扇区为单位）

参数：无

返回值：坏扇区数（通常会对齐到簇的整数倍）
--*/
{
    return m_BlockInforHead.BadBlockSize.QuadPart;
}

MESSAGE_CALLBACK_FUNC CRepairController::RegisterMessageCallBack( MESSAGE_CALLBACK_FUNC lpFn )
/*++
功能描述：注册消息通知函数回调

参数：
    lpFn:被注册的函数指针，原型为MESSAGE_CALLBACK_FUNC

返回值：当前被注册的消息通知函数指针
--*/
{
    MESSAGE_CALLBACK_FUNC prevFunc = m_lpMessageFunc;
    m_lpMessageFunc = lpFn;
    return prevFunc;
}

MESSAGE_CALLBACK_FUNC CRepairController::UnregisterMessageCallBack()
/*++
功能描述：撤消注册消息通知函数

参数：无

返回值：当前已经注册的回调函数指针，若不存在返回NULL
--*/
{
    MESSAGE_CALLBACK_FUNC prevFunc = m_lpMessageFunc;
    m_lpMessageFunc = NULL;
    return prevFunc;
}

VOID CRepairController::ReportStateMessage( LPSTR message )
/*++
功能描述：调用注册的回调函数显示当前状态

参数：指向要显示的状态字符串

返回值：无
--*/
{
    if( m_lpMessageFunc == NULL )return;

    m_lpMessageFunc( MESSAGE_CODE_REPORTSTATE,(DWORD_PTR)message,0);
}

VOID CRepairController::ReportFileNameMessage(LPWSTR FileName)
/*++
功能描述：调用注册的回调函数显示文件名

参数：
    FileName:指向要显示的状态字符串(宽字符集）

返回值：无
--*/
{
    if( m_lpMessageFunc == NULL )return;

    m_lpMessageFunc( MESSAGE_CODE_FILENAME,(DWORD_PTR)FileName,0);
}

VOID CRepairController::ReportErrorMessage( LPSTR message )
/*++
功能描述：调用注册的回调函数显示当前出错消息

参数：指向要显示的错误消息字符串

返回值：无
--*/
{
    if( m_lpMessageFunc == NULL )return;

    m_lpMessageFunc( MESSAGE_CODE_REPORTERROR,(DWORD_PTR)message,0);
}

VOID CRepairController::ReportProgressState(DWORD Curr,DWORD Total )
/*++
功能描述：调用注册的回调函数显示当前进度

参数：
    Cur:当前的进度值
    Total:总进度值

返回值：无
--*/
{
    if( m_lpMessageFunc == NULL )return;

    m_lpMessageFunc( MESSAGE_CODE_PROGRESS,(DWORD_PTR)Curr,(DWORD_PTR)Total);
}

VOID CRepairController::ReportNotifyMessage()
/*++
功能描述：发送通知消息（证明程序在运行中）

参数：无

返回值：无
--*/
{
    if( m_lpMessageFunc == NULL )return;

    m_lpMessageFunc( MESSAGE_CODE_NOTIFY,0,0);

}

BOOL CRepairController::ProbeForRepair()
/*++
功能描述：探测是否符合修复条件

参数：无

返回值：符合返回TRUE，否则返回FALSE

注意：为纯虚函数，子类实现
--*/
{
    return FALSE;
}

VOID CRepairController::AddBadBlock( LONGLONG Lcn,LONGLONG NumberOfSectors )
/*++
功能描述：添加一个坏块区域，更新内部的链表结构

参数：
    StartLcn:起始逻辑扇区号（Lcn)
    NumberOfSectors:坏块区包含的扇区数

返回值：无

注意：纯虚函数，子类实现
--*/
{

}

VOID AddDeadBlock( LONGLONG StartLsn,LONGLONG NumberOfSectors )
/*++
功能描述：添加一个死块区域（硬故障扇区，不可读），更新内部的链表结构

参数：
    StartLsn:起始逻辑扇区号（Lsn)
    NumberOfSectors:坏块区包含的扇区数

返回值：无

注意：纯虚函数，子类实现

--*/
{

}
BOOL CRepairController::ReadLogicalSector(OUT LPVOID buffer,
                     IN DWORD bufferSize,
                     IN LONGLONG Lsn
                     )
/*++
功能描述：读逻辑扇区（扇区号相对于分区开始编址）

参数：
    hDisk: 磁盘设备句柄（必须具有读权限）
    buffer:输出缓冲区，大小至少为一个扇区
    bufferSize:输出缓冲区buffer的大小
    Lsn:逻辑扇区号

返回值：成功返回TRUE，失败返回FALSE

--*/
{
    LONGLONG tmp = m_VolumeStartSector.QuadPart + Lsn;
    assert( tmp > 0 );
    assert( Lsn < m_VolumeTotalSectors.QuadPart );

    return ReadSector( m_hDisk,buffer,bufferSize,(DWORD)(tmp & 0xffffffff),(DWORD)(tmp >> 32) );
}

BOOL CRepairController::WriteLogicalSector( IN LPVOID buffer,
                     IN DWORD bufferSize,
                     IN LONGLONG Lsn
                     )
/*++
功能描述：写逻辑扇区（扇区号相对于分区开始编址）

参数：
    hDisk: 磁盘设备句柄（必须具有写权限）
    buffer:输入缓冲区，大小至少为一个扇区
    bufferSize:输入缓冲区buffer的大小
    Lsn:逻辑扇区号

返回值：成功返回TRUE，失败返回FALSE
--*/
{
    LONGLONG tmp = m_VolumeStartSector.QuadPart + Lsn;
    assert( tmp > 0 );
    assert( Lsn < m_VolumeTotalSectors.QuadPart );

    return WriteSector( m_hDisk,buffer,bufferSize,(DWORD)(tmp & 0xffffffff),(DWORD)(tmp >> 32) );
}

