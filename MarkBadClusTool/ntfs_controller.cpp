//ntfs_controller.cpp for class-- CNtfsController
//author:lzc
//date:2012/11/14
//e-mail:hackerlzc@126.com

#include "stdafx.h"
#include<windows.h>
#include"ntfs_controller.h"
#include<map>
using namespace std;
//类实现

CNtfsController::CNtfsController(LPSTR lpszDiskPath, LONGLONG StartSector, LONGLONG NumberOfSectors)
/*++
功能描述:构造函数

参数:无

返回值:无

--*/
:CRepairController( lpszDiskPath,StartSector,NumberOfSectors ),
m_MftDataRuns(NULL),
m_MftDataRunsLength(0),
m_MftRecordLength(0),
m_Bitmap(NULL),
m_BitmapLength(0),
m_MftBitmap(NULL),
m_MftBitmapLength(0)
{
    DbgPrint("CNtfsController constructor called!");
    BOOL    bOk = FALSE;

    //初始化NTFS的DBR扇区
    bOk = ReadLogicalSector( &m_BootSect,sizeof(m_BootSect),0);
    assert( bOk );
    m_ClusterSizeInBytes = m_BootSect.bpb.sectors_per_cluster * m_BootSect.bpb.bytes_per_sector;

    if( !InitController()) {
        DbgPrint("init controller failed!");
    }
}

CNtfsController::~CNtfsController()
/*++
功能描述:析构函数
--*/
{
    DbgPrint("CNtfsController destructor called!");

    ReleaseResources();
}

VOID CNtfsController::PrepareUpdateBadBlockList()
/*++
功能描述：准备更新坏块链表

参数：无

返回值：无

说明：在每次要刷新坏块链表前都要调用此函数
--*/
{
    DbgPrint("prepare update badblocklist!");

    DestroyListNodes( &m_BlockInforHead.BadBlockList);
    m_BlockInforHead.BadBlockSize.QuadPart = 0;

    LONGLONG last_value=0;                  //用于检查BadBlockList是升序排序的断言

    NTFS_FILE   file = OpenNtfsFile( FILE_BadClus );
    if( file == NULL )
        goto exit;

    DWORD valueLength = GetAttributeValue( file,AT_DATA,NULL,0,NULL,L"$Bad");
    if( valueLength == -1 )
        goto exit;
    assert( valueLength > 0 );
    
    LPBYTE buffer = (LPBYTE)malloc( valueLength );
    assert( buffer != NULL);
    BOOL bDataruns = FALSE;
    if( 0 != GetAttributeValue( file,AT_DATA,buffer,valueLength,&bDataruns,L"$Bad"))
        goto exit;
    assert( bDataruns == TRUE );

    //从Dataruns中提取坏块信息

    LONGLONG startLcn = 0,len = 0;
    for(DWORD i = 0;
        i < valueLength;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( buffer[i] == 0 )break;

        cStartLcnBytes = ( buffer[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (buffer[i] & 0x0f);        //压缩字节的低4位

        //提取当前数据流长度
        len = 0;
        for( DWORD j = cLenLcnBytes;j > 0;j--)
        {
            len = (len << 8) | buffer[i + j ]; 
        }

        //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
        LONGLONG tmp = 0;
        if( buffer[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
            tmp = -1ll;
        for( DWORD j = cStartLcnBytes;j > 0;j-- )
        {
            tmp = ( tmp << 8 ) | buffer[i + cLenLcnBytes + j ];
        }
        startLcn = startLcn + tmp;
        if( cStartLcnBytes > 0 )
        {
            assert( startLcn > last_value );
            last_value = startLcn;
            FreeBlock( startLcn ,len);
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }
exit:
    if( file != NULL)CloseFile( file );
    file = NULL;

    if( buffer != NULL)free(buffer);
    buffer = NULL;
    return;
}

VOID CNtfsController::AddBadBlock( LONGLONG StartLsn,LONGLONG NumberOfSectors )
/*++
功能描述:向坏块链表中增加描述结点,并调整Bitmap

参数
    StartLsn:起始逻辑扇区号
    NumberOfSectors:扇区数

返回值:无

说明：须保证链表按块首Lsn升序排序

--*/
{
    //调整到簇对齐
    LONGLONG sectors_per_cluster = m_BootSect.bpb.sectors_per_cluster;
    StartLsn = StartLsn / sectors_per_cluster * sectors_per_cluster;
    NumberOfSectors = NumberOfSectors % sectors_per_cluster == 0?
                        NumberOfSectors:(NumberOfSectors/sectors_per_cluster+1)*sectors_per_cluster;
#if 0
    PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof(BLOCK_DESCRIPTOR));
    assert( node != NULL );
    RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR));
    node->StartSector.QuadPart = StartLsn;
    node->TotalSectors.QuadPart = NumberOfSectors;
    node->type = BLOCK_TYPE_BAD;
    m_BlockInforHead.BadBlockSize.QuadPart += NumberOfSectors * m_BootSect.bpb.bytes_per_sector;
    InsertTailList( &m_BlockInforHead.BadBlockList,&node->List);
    node = NULL;
#endif

    //调整Bitmap
    for(LONGLONG i = StartLsn / sectors_per_cluster;
        i < (StartLsn + NumberOfSectors) / sectors_per_cluster;
        i++)
    {
        m_Bitmap[ i / 8 ] |= (1 << (i % 8));
    }

    //升序插入到坏块链表
 
    if( IsListEmpty( &m_BlockInforHead.BadBlockList))
    {
        PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
        assert( node != NULL );

        RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
        node->StartSector.QuadPart = StartLsn;
        node->TotalSectors.QuadPart = NumberOfSectors;
        node->type = BLOCK_TYPE_BAD;
        InsertHeadList( &m_BlockInforHead.BadBlockList,&node->List );
        node = NULL;
        return;
    }

    PLIST_ENTRY list = NULL;
    PBLOCK_DESCRIPTOR first_block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(
        m_BlockInforHead.BadBlockList.Flink,
        BLOCK_DESCRIPTOR,
        List);
    PBLOCK_DESCRIPTOR last_block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(
            m_BlockInforHead.BadBlockList.Blink,
            BLOCK_DESCRIPTOR,
            List);

    if( StartLsn <= first_block->StartSector.QuadPart )
    {
        if( StartLsn + NumberOfSectors >= 
            first_block->StartSector.QuadPart )
        {
            first_block->TotalSectors.QuadPart = 
                max(first_block->StartSector.QuadPart + first_block->TotalSectors.QuadPart,
                    StartLsn + NumberOfSectors)
                - StartLsn;
            first_block->StartSector.QuadPart = StartLsn;
            
            PBLOCK_DESCRIPTOR block_prev = first_block;
            PBLOCK_DESCRIPTOR block_next =(PBLOCK_DESCRIPTOR)CONTAINING_RECORD(block_prev->List.Flink,
                                                BLOCK_DESCRIPTOR,
                                                List );
            while( (&m_BlockInforHead.BadBlockList != &block_next->List) &&
                (block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart
                >= block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart) )
            {
                PBLOCK_DESCRIPTOR tmp = block_next;
                block_next = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(block_next->List.Flink,
                                                                BLOCK_DESCRIPTOR,
                                                                List );
                RemoveEntryList( &tmp->List );
                free( tmp );tmp = NULL;
            }
            if((&m_BlockInforHead.BadBlockList != &block_next->List) && 
                ( block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart
                >= block_next->StartSector.QuadPart))
            {
                block_prev->TotalSectors.QuadPart = 
                    max( block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart,
                        block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart)
                    - block_prev->StartSector.QuadPart;
                RemoveEntryList( &block_next->List );
                free( block_next );
            }
        }
        else
        {
            PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
            assert( node != NULL );

            RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
            node->StartSector.QuadPart = StartLsn;
            node->TotalSectors.QuadPart = NumberOfSectors;
            node->type = BLOCK_TYPE_BAD;
            InsertHeadList( &m_BlockInforHead.BadBlockList,&node->List );
            node = NULL;
        }
    }
    else if(StartLsn >= last_block->StartSector.QuadPart)
    {
        if( (last_block->StartSector.QuadPart + last_block->TotalSectors.QuadPart)
            >= StartLsn )
        {
            last_block->TotalSectors.QuadPart =
                max(StartLsn + NumberOfSectors,
                    last_block->StartSector.QuadPart + last_block->TotalSectors.QuadPart)
                - last_block->StartSector.QuadPart;
        }
        else
        {
            PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
            assert( node != NULL );

            RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
            node->StartSector.QuadPart = StartLsn;
            node->TotalSectors.QuadPart = NumberOfSectors;
            node->type = BLOCK_TYPE_BAD;
            InsertTailList( &m_BlockInforHead.BadBlockList,&node->List );
            node = NULL;
        }
    }
    else{

        for( list = m_BlockInforHead.BadBlockList.Flink;
            list != &m_BlockInforHead.BadBlockList;
            )
        {
            //上边已经排除这种情况
            assert( list != m_BlockInforHead.BadBlockList.Blink );

            PBLOCK_DESCRIPTOR block_prev = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                        BLOCK_DESCRIPTOR,
                                                                        List);
            list = list->Flink;
            PBLOCK_DESCRIPTOR block_next = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                            BLOCK_DESCRIPTOR,
                                                                            List);
            //寻找插入点
            if( StartLsn > block_next->StartSector.QuadPart )
                continue;
                
            //处理归并块的四种情况

            BOOLEAN bAdjPrev = (
                StartLsn > block_prev->StartSector.QuadPart &&
                StartLsn <= (block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart)
                );
            BOOLEAN bAdjNext = (
                StartLsn + NumberOfSectors >= block_next->StartSector.QuadPart /*&&
                StartLsn + NumberOfSectors < (block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart)*/
                );

            if( bAdjPrev && bAdjNext )
            {
                //前后均相邻
                block_prev->TotalSectors.QuadPart = 
                    max( block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart,
                        StartLsn + NumberOfSectors )
                    - block_prev->StartSector.QuadPart;
                while( (&m_BlockInforHead.BadBlockList != &block_next->List) &&
                    (block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart
                    >= block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart) )
                {
                    PBLOCK_DESCRIPTOR tmp = block_next;
                    block_next = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(block_next->List.Flink,
                                                                    BLOCK_DESCRIPTOR,
                                                                    List );
                    RemoveEntryList( &tmp->List );
                    free( tmp );tmp = NULL;
                }
                if((&m_BlockInforHead.BadBlockList != &block_next->List) && 
                    ( block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart
                    >= block_next->StartSector.QuadPart))
                {
                    block_prev->TotalSectors.QuadPart = 
                        max( block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart,
                            block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart)
                            - block_prev->StartSector.QuadPart;
                    RemoveEntryList( &block_next->List );
                    free( block_next );
                }
            }
            else if ( bAdjPrev )
            {
                block_prev->TotalSectors.QuadPart = 
                    (StartLsn + NumberOfSectors) - block_prev->StartSector.QuadPart;
            }
            else if( bAdjNext )
            {
                block_next->TotalSectors.QuadPart = 
                    max( block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart,
                         StartLsn + NumberOfSectors )
                    - StartLsn;
                block_next->StartSector.QuadPart = StartLsn;

                block_prev = block_next;
                block_next =(PBLOCK_DESCRIPTOR)CONTAINING_RECORD(block_prev->List.Flink,
                                                BLOCK_DESCRIPTOR,
                                                List );
                while( (&m_BlockInforHead.BadBlockList != &block_next->List) &&
                    (block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart
                    >= block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart) )
                {
                    PBLOCK_DESCRIPTOR tmp = block_next;
                    block_next = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(block_next->List.Flink,
                                                                    BLOCK_DESCRIPTOR,
                                                                    List );
                    RemoveEntryList( &tmp->List );
                    free( tmp );tmp = NULL;
                }
                if((&m_BlockInforHead.BadBlockList != &block_next->List) && 
                    ( block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart
                    >= block_next->StartSector.QuadPart))
                {
                    block_prev->TotalSectors.QuadPart = 
                        max( block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart,
                            block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart)
                            - block_prev->StartSector.QuadPart;
                    RemoveEntryList( &block_next->List );
                    free( block_next );
                }
            }
            else
            {
                PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
                assert( node != NULL );

                RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
                node->StartSector.QuadPart = StartLsn;
                node->TotalSectors.QuadPart = NumberOfSectors;
                node->type = BLOCK_TYPE_BAD;
                InsertHeadList( &block_prev->List,&node->List );
                node = NULL;
            }// end if bAdj....
                
            break;
        }// end for list

    }//else end

    //_ShowList();
}

VOID CNtfsController::AddDeadBlock( LONGLONG StartLsn,LONGLONG NumberOfSectors )
/*++
功能描述:向死块链表中增加描述结点

参数
    StartLsn:起始逻辑扇区号
    NumberOfSectors:扇区数

返回值:无

说明:死块是指扇区完全损坏的区域,无法访问。对应MHDD一类软件中表示为ERROR的区域
--*/
{
    PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof(BLOCK_DESCRIPTOR));
    assert( node != NULL );
    RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR));
    node->StartSector.QuadPart = StartLsn;
    node->TotalSectors.QuadPart = NumberOfSectors;
    node->type = BLOCK_TYPE_BAD;
    InsertTailList( &m_BlockInforHead.UsedBlockList,&node->List);
    node = NULL;

}

BOOL CNtfsController::ProbeForRepair()
/*++
功能描述:检测是否符合本软件要求的最低条件

参数:无

返回值:符合返回TRUE,失败返回FALSE
--*/
{
    BOOL bResult = TRUE;

    //校验文件系统
    if( !VerifyFileSystem() )
    {
        bResult = FALSE;
        goto exit;
    }



exit:
    return bResult;
}

extern FILE *hLogFile;
VOID CNtfsController::_ShowList()
{
#if 1
    fprintf(hLogFile,"////////////////////////////////////////////\n");
    fprintf(hLogFile,"Bad Block List\n");
    for( PBLOCK_DESCRIPTOR p = GetFirstBadBlock();
        p != NULL;
        p = GetNextBadBlock( p ))
    {
        fprintf(hLogFile,"start:%lld,len:%lld\n",p->StartSector.QuadPart,p->TotalSectors.QuadPart );
    }
    fprintf(hLogFile,"========================================\n");
#endif

#if 1
    fprintf(hLogFile,"Free Block List size = %.1lf GB\n",(double)(GetFreeBlockSize()/1024/1024)/1024);
    for( PBLOCK_DESCRIPTOR p = GetFirstFreeBlock();
        p != NULL;
        p = GetNextFreeBlock( p ))
    {
        fprintf(hLogFile,"start:%lld,len:%lld\n",p->StartSector.QuadPart,p->TotalSectors.QuadPart );
    }
    fprintf(hLogFile,"========================================\n");

    fprintf(hLogFile,"Used Block List size =%.1lf GB \n",(double)(GetUsedBlockSize()/1024/1024)/1024);
    for( PBLOCK_DESCRIPTOR p = GetFirstUsedBlock();
        p != NULL;
        p = GetNextUsedBlock( p ))
    {
        fprintf(hLogFile,"start:%lld,len:%lld\n",p->StartSector.QuadPart,p->TotalSectors.QuadPart );
    }
    fprintf(hLogFile,"========================================\n");
#endif
    fprintf(hLogFile,"////////////////////////////////////////////\n");

}

BOOL CNtfsController::VerifyFileSystem()
/*++
功能描述：校验文件系统的正确性

参数：无

返回值：文件系统符合软件工作条件返回TRUE，否则返回FALSE

--*/
{
    ReportStateMessage("正在校验文件系统...   ");
 
    BOOL bResult = FALSE;
    LPBYTE dataruns = NULL;
    LONG len_dataruns = 0;

    LONGLONG sum = 0;
    for( LONGLONG i = 0;
        i < (m_MftNumberOfRecord % 8==0?m_MftNumberOfRecord/8:m_MftNumberOfRecord/8 + 1);
        i++)
    {
        BYTE byte = m_MftBitmap[i];
        for( int j = 0;j < 8;j++)
        {
            LONGLONG file_id = i * 8 + j;
            ReportProgressState( (DWORD)file_id+1,(DWORD)m_MftNumberOfRecord );

            if( byte & (1 << j))
            {
                NTFS_FILE hFile = OpenNtfsFile( file_id );
                if( hFile == NULL)
                {
                    continue;
                }
                
                for( PLIST_ENTRY list = hFile->List.Flink;
                    list != &hFile->List;
                    list = list->Flink)
                {
                    PFILE_ATTRIBUTE_NODE node = (PFILE_ATTRIBUTE_NODE)CONTAINING_RECORD(
                                                                list,
                                                                FILE_ATTRIBUTE_NODE,
                                                                List);
                    assert( node != NULL);
                    PATTR_RECORD pAttrRecord = (PATTR_RECORD)node->AttributeData;
                    if( pAttrRecord->non_resident == 0 )
                        continue;

                    dataruns = (LPBYTE)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset);
                    len_dataruns = node->Length - pAttrRecord->mapping_pairs_offset;
                    sum += GetNumberOfVcnsInDataRuns( dataruns,len_dataruns )
                          * m_ClusterSizeInBytes;
                    dataruns = NULL;len_dataruns = 0;
                }
                CloseFile( hFile );
            }
        }
    }
 
    //printf("\nsum = %lld\n",sum);
    bResult = ( sum == GetUsedBlockSize());
    if( bResult )
    {
        ReportStateMessage("\n文件系统校验成功！\n");
        //_ShowList();
    }
    else
        ReportStateMessage("\n文件系统校验失败,请运行Chkdsk修复错误！\n");


    if( dataruns != NULL)free( dataruns );

    return bResult;
}

BOOL CNtfsController::StartRepairProgress()
/*++
功能描述:开始修复进程

参数:无

返回值:修复成功返回TRUE,失败返回FALSE
--*/
{
    DbgPrint("Show list...");
    _ShowList();
    InitFreeAndUsedBlockList();
    DbgPrint("Show list after init again...");
    _ShowList();

    BOOL bResult = FALSE;
    LONG retValue=0;

    ReportStateMessage("正在检查受影响文件...   ");

    //检测所有文件记录是否受坏块影响需要移动数据
    //注意：最后处理$Mft,$Bitmap,因为这几个文件影响其它文件的修复
    //不处理$Boot文件

    for( LONGLONG i = 0;
        i < (m_MftNumberOfRecord % 8==0?m_MftNumberOfRecord/8:m_MftNumberOfRecord/8 + 1);
        i++)
    {
        BYTE byte = m_MftBitmap[i];
        for( int j = 0;j < 8;j++)
        {
            LONGLONG file_id = i * 8 + j;
            ReportProgressState( (DWORD)file_id+1,(DWORD)m_MftNumberOfRecord );
            if( file_id == FILE_MFT || file_id == FILE_Bitmap || file_id == FILE_BadClus || file_id == FILE_Boot)
                continue;

            if( byte & (1 << j))
            {
                CHAR buffer[256];
                retValue = CheckAndUpdateFile(file_id); 
                if( 1 == retValue)
                {
                    sprintf_s(buffer,256,"文件 %lld 受影响，并且移动成功",file_id );
                }
                else if( 2 == retValue )
                {
                    sprintf_s(buffer,256,"文件 %lld 受影响，但移动失败,文件名已经写入日志!",file_id );
                }
                else if( retValue < 0 )
                {
                    sprintf_s(buffer,256,"文件 %lld 非法",file_id );
                }
                if( retValue != 0 )
                    ReportStateMessage( buffer );
            }
        }
    }

    //处理$Bitmap文件
    ReportStateMessage("正在检查卷空间位图文件...   ");
    retValue = CheckAndUpdateFile( FILE_Bitmap );
    if( retValue == 1 )
    {
        DbgPrint("Bitmap file is influenced and updated!");
        ReportStateMessage("卷空间位图文件更新成功完成！   ");
    }
    else if( retValue == 2 )
    {
        DbgPrint("Bitmap file is influenced and updated failed!");
        ReportErrorMessage("卷空间位图文件更新失败！   ");
        goto exit;
    }
    else if( retValue == 0 )
    {
        DbgPrint("Bitmap file is not influenced.");
        ReportStateMessage("卷空间位图文件未受影响！   ");
    }
    else 
    {
        DbgPrint("Bitmap file is illegal");
        ReportErrorMessage("卷空间位图文件非法!   ");
        goto exit;
    }

    //处理$Mft文件
    ReportStateMessage("正在检查$Mft文件...   ");
    retValue = this->CheckAndUpdateFile( FILE_MFT );
    if( retValue == 1 )
    {
        DbgPrint("Bitmap file is influenced and updated!");
        ReportStateMessage("$Mft文件更新成功完成！   ");
    }
    else if( retValue == 2 )
    {
        DbgPrint("$Mft file is influenced and updated failed!");
        ReportErrorMessage("$Mft文件更新失败！   ");
        goto exit;
    }
    else if( retValue == 0 )
    {
        DbgPrint("$Mft file is not influenced.");
        ReportStateMessage("$Mft文件未受影响！   ");
    }
    else 
    {
        DbgPrint("$Mft file is illegal");
        ReportErrorMessage("$Mft文件非法!   ");
        goto exit;
    }

    //更新$BadClus 文件
    ReportStateMessage("正在更新$BadClus文件...   ");
    bResult = UpdateBadClus();
    if(bResult)
    {
        ReportStateMessage("$BadClus文件更新完成！");
    }
    else
    {
        ReportStateMessage("$BadClus文件更新失败！");
        goto exit;
    }

    //更新磁盘位图
    ReportStateMessage("正在写入新的卷位图...   ");
    bResult = UpdateBitmap();
    if(bResult)
    {
        ReportStateMessage("卷位图写入完成!");
    }
    else
    {
        ReportErrorMessage("卷位图写入失败！");
        goto exit;
    }

    //更新文件记录分配位图
    ReportStateMessage("正在写入新的文件记录分配位图...   ");
    bResult = UpdateMftBitmap();
    if(bResult)
    {
        ReportStateMessage("文件记录分配位图写入完成!");
    }
    else
    {
        ReportErrorMessage("文件记录分配位图写入失败！");
        goto exit;
    }

    //更新引导扇区数据
    ReportStateMessage("正在更新引导扇区...   ");
    bResult = WriteLogicalSector( &m_BootSect,sizeof(m_BootSect),0);
    if( !bResult )
    {
        DbgPrint("update boot sector failed!");
        ReportErrorMessage("引导扇区更新失败!   ");
        goto exit;
    }
    else
        ReportStateMessage("引导扇区更新完成   ");

    //更新$MFTMirr文件,使之内容与$MFT文件同步
    ReportStateMessage("正在同步$MFTMirr文件...   ");
    bResult = UpdateMftMirr();
    if( !bResult )
    {
        DbgPrint("update $MFTMirr failed!");
        ReportErrorMessage("同步$MFTMirr文件失败!   ");
        goto exit;
    }
    else
    {
        ReportStateMessage("同步$MFTMirr文件完成！");
    }
    bResult = TRUE;
    
    ReportStateMessage("修复完成！");
exit:

    return bResult;
}

BOOL CNtfsController::StopRepairProgress()
/*++
功能描述:终止修复进程

参数:无

返回值:终止成功返回TRUE,失败返回FALSE
--*/
{
    return TRUE;
}

BOOL CNtfsController::InitController()
/*++
功能描述:初始化Controller,不同的文件系统有不同的初始化方法,真正在子类中
         实现此接口,用来实现对不同文件系统的支持。

参数:无

返回值:成功返回TRUE,失败返回FALSE

说明:本方法是NTFS对InitController的具体实现,功能包括初始化内部相关的链表等
--*/
{
    BOOL    bStatus = TRUE;

    DbgPrint("init controller is called!\n");

    ReleaseResources();

    //初始化寻址$MFT要用到的关键变量

    //文件记录大小
    if( m_BootSect.clusters_per_mft_record < 0 )
        m_MftRecordLength = 1 << -1 * m_BootSect.clusters_per_mft_record;
    else
        m_MftRecordLength = m_BootSect.clusters_per_mft_record
        * m_ClusterSizeInBytes;

    //初始化描述$MFT的文件记录缓冲区
    LPBYTE  clusterBuffer = (LPBYTE)malloc( m_ClusterSizeInBytes );
    assert( clusterBuffer != NULL);
    bStatus = ReadLogicalCluster( clusterBuffer,m_ClusterSizeInBytes,
        m_BootSect.mft_lcn );
    if( !bStatus )
        goto exit;
    bStatus = ntfs_is_file_recordp( clusterBuffer );
    if( !bStatus )
        goto exit;

    //初始化$MFT文件,目的是初始化一些类成员变量（eg,m_MftDataRuns...）,为进一步对NTFS的文件操作做准备
    bStatus = InitMftFile( clusterBuffer,m_ClusterSizeInBytes );
    if( !bStatus )
        goto exit;

    bStatus = InitBitmap();
    if ( !bStatus )
        goto exit;

    bStatus = InitMftBitmap();
    if( !bStatus )
        goto exit;

    bStatus = InitFreeAndUsedBlockList();
    if( !bStatus )
        goto exit;

    bStatus = InitBadBlockList();
    if( !bStatus )
        goto exit;

    //========================================================================================

    DbgPrint("ShowList....");
    _ShowList();

exit:
    if( clusterBuffer != NULL)
    {
        free( clusterBuffer );
        clusterBuffer = NULL;
    }

    return bStatus;
}



VOID CNtfsController::ReleaseResources()
/*++
功能描述:释放本类中申请的资源

参数:无

返回值:无
--*/
{
    DbgPrint("called!");

    DestroyListNodes( &m_BlockInforHead.BadBlockList);
    DestroyListNodes( &m_BlockInforHead.DeadBlockList);
    DestroyListNodes( &m_BlockInforHead.FreeBlockList);
    DestroyListNodes( &m_BlockInforHead.UsedBlockList);
    m_BlockInforHead.BadBlockSize.QuadPart = 
        m_BlockInforHead.FreeBlockSize.QuadPart = 
        m_BlockInforHead.UsedBlockSize.QuadPart = 0;

    if( m_MftDataRuns != NULL){
        free( m_MftDataRuns );
        m_MftDataRuns = NULL;
    }

    if( m_Bitmap != NULL){
        free( m_Bitmap );
        m_Bitmap = NULL;
    }

    if( m_MftBitmap != NULL)
    {
        free( m_MftBitmap );
        m_MftBitmap = NULL;
    }

}

BOOL CNtfsController::InitFreeAndUsedBlockList()
/*++
功能描述:从NTFS卷的位图中获得未分配和已分配扇区的描述信息

参数:无

返回值:成功返回TRUE,失败返回FALSE

说明:位图文件中是以簇为单位的,要换算为扇区。
    本函数要建立父类相应的链表( m_BlockInforHead )
--*/
{
    LONGLONG currBlockStart = 0,currBlockLen = 0;   //单位为簇
    DWORD bit_test = 1;                             //用于位测试

    //销毁原有链表,准备重新初始化
    DestroyListNodes( &m_BlockInforHead.FreeBlockList);
    DestroyListNodes( &m_BlockInforHead.UsedBlockList);
    m_BlockInforHead.FreeBlockSize.QuadPart = 
        m_BlockInforHead.UsedBlockSize.QuadPart = 0;

    BYTE type = ((*(PDWORD)m_Bitmap & bit_test) != 0);     //1为使用,0为未用
    BYTE tmp = m_Bitmap[ m_BitmapLength - 1];
    m_Bitmap[ m_BitmapLength - 1] &= 0x7f;//用于建立空闲和占用块链表时方便处理最后一个占用块。

    for( LPBYTE p = m_Bitmap;
           p < m_Bitmap + m_BitmapLength;
           )          //m_BitmapLength是8字节对齐
    {
        if( ((*(PDWORD)p & bit_test) != 0) != type )
        {
            //printf( "type:%d,start:%lld,len:%lld\n",type,currBlockStart,currBlockLen );
            
            //初始化信息块描述结点,插入链表中
            PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof(BLOCK_DESCRIPTOR));
            assert( node != NULL );
            RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
            node->StartSector.QuadPart = currBlockStart * m_BootSect.bpb.sectors_per_cluster;
            node->TotalSectors.QuadPart = currBlockLen * m_BootSect.bpb.sectors_per_cluster;
            if( type )
            {
                node->type = BLOCK_TYPE_USED;
                InsertTailList( &m_BlockInforHead.UsedBlockList,&node->List);
                node = NULL;
                m_BlockInforHead.UsedBlockSize.QuadPart += m_ClusterSizeInBytes * currBlockLen;
            }
            else
            {
                node->type = BLOCK_TYPE_FREE;
                InsertTailList( &m_BlockInforHead.FreeBlockList,&node->List);
                node = NULL;
                m_BlockInforHead.FreeBlockSize.QuadPart += m_ClusterSizeInBytes * currBlockLen;
            }

            type = ((*(PDWORD)p & bit_test) != 0);
            currBlockStart = currBlockStart + currBlockLen;
            currBlockLen = 0;
        }

        if( bit_test == 1 && (*(PDWORD)p == 0xfffffffful ||  *(PDWORD)p == 0) )
        {
            currBlockLen += 32;
            p += 4;
            continue;
        }
           
        for( ;bit_test != 0;bit_test <<=1  )
        {
            if( ((*(PDWORD)p & bit_test) != 0) != type )
                break;

            currBlockLen++;
        }
        if( bit_test == 0)
        {
            bit_test = 1;
            p += 4;
        }
    }
    m_Bitmap[ m_BitmapLength - 1] = tmp;

    //调整最后一个占用块的大小到正确值
    PLIST_ENTRY list = m_BlockInforHead.UsedBlockList.Blink;
    PBLOCK_DESCRIPTOR p = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                BLOCK_DESCRIPTOR,
                                                                List);
    p->TotalSectors.QuadPart -= (m_BitmapLength * 8 - 
                                m_BootSect.number_of_sectors / m_BootSect.bpb.sectors_per_cluster//有效簇数
                                - 1) * m_BootSect.bpb.sectors_per_cluster;
    m_BlockInforHead.UsedBlockSize.QuadPart -= 
                                    (m_BitmapLength * 8 - 
                                    m_BootSect.number_of_sectors / m_BootSect.bpb.sectors_per_cluster//有效簇数
                                    - 1) * m_ClusterSizeInBytes;
    if( p->TotalSectors.QuadPart == 0 )
    {
        RemoveEntryList( &p->List );
        free( p );
    }

    assert(  type == 0 );
    return TRUE;
}


VOID CNtfsController::DestroyListNodes(PLIST_ENTRY ListHead )
/*++
功能描述:释放块描述信息链表的结点内存

参数 ListHead:链表头结点指针

返回值:无
--*/
{
    while( !IsListEmpty( ListHead ))
    {
        PLIST_ENTRY list = RemoveHeadList( ListHead );
        PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
            BLOCK_DESCRIPTOR,
            List);
        assert( node );
        free(node);
    }
}

BOOL CNtfsController::ReadLogicalCluster( OUT LPVOID Buffer,
                                         IN DWORD BufferSize,
                                         IN LONGLONG Lcn,
                                         IN DWORD TryTime,
                                         IN BYTE BadByte)
/*++
功能描述:读取逻辑簇

参数:
    Buffer:输出缓冲区,至少一簇的大小
    BufferSize:指出输出缓冲区的大小,至少为一簇
    Lcn:逻辑簇号
    TryTime:扇区读取失败后重试次数,默认为1次
    BadByte:最终扇区读取失败后的填充字符

返回值:成功（一簇的所有扇区均正确读取）返回TRUE,失败返回FALSE

--*/
{
    BOOL bOk = FALSE,bResult = TRUE;
    DWORD   sector_size = m_BootSect.bpb.bytes_per_sector;
    DWORD   sectors_per_cluster = m_BootSect.bpb.sectors_per_cluster;

    assert( BufferSize >= sector_size * sectors_per_cluster );

    for( DWORD i = 0;i < sectors_per_cluster;i++)
    {
        for( DWORD count = 0;count < TryTime;count++)
        {
            bOk = ReadLogicalSector( (LPBYTE)Buffer + i * sector_size,
                sector_size,
                Lcn * sectors_per_cluster + i);
            if( bOk )break;
        }
        if( !bOk )
        {
            memset( (LPBYTE)Buffer + i * sector_size,BadByte,sector_size );
            bResult = FALSE;
        }
    }

    return bResult;
}

BOOL CNtfsController::WriteLogicalCluster( IN LPVOID Buffer,
                                          IN DWORD DataLen,
                                          IN LONGLONG Lcn,
                                          IN DWORD TryTime /*= 1*/)
/*++
功能描述:向卷写入逻辑簇

参数:
    Buffer:输入缓冲区
    DataLen:数据长度（字节）,不超过一簇大小
    Lcn:逻辑簇号
    TryTime:扇区读取失败后重试次数,默认为1次

返回值:成功（一簇的所有扇区均正确写入）返回TRUE,失败返回FALSE
--*/
{
    BOOL bOk = FALSE,bResult = TRUE;
    DWORD   sector_size = m_BootSect.bpb.bytes_per_sector;
    DWORD   sectors_per_cluster = m_BootSect.bpb.sectors_per_cluster;

    assert( DataLen <= sector_size * sectors_per_cluster );
    BYTE *buf = (BYTE *)malloc( sector_size * sectors_per_cluster );
    assert( buf != NULL);
    RtlZeroMemory( buf,sector_size * sectors_per_cluster );
    memcpy( buf,Buffer,DataLen );

    for( DWORD i = 0;i < sectors_per_cluster;i++)
    {
        for( DWORD count = 0;count < TryTime;count++)
        {
            bOk = WriteLogicalSector( (LPBYTE)buf + i * sector_size,
                sector_size,
                Lcn * sectors_per_cluster + i);
            if( bOk )break;
        }
        if( !bOk )
        {
            char buffer[256];
            sprintf_s( buffer,256,"write logical sector %d failed!\n",
                Lcn * sectors_per_cluster + i );
            ReportErrorMessage( buffer );
            bResult = FALSE;
        }
    }
    free( buf );

    return bResult;
}

BOOL CNtfsController::CopyLogicalClusterBlock( LONGLONG SourceLcn,LONGLONG DestLcn,LONGLONG NumberOfLcns)
/*++
--*/
{
    BOOL bOk = TRUE;
    bOk = CopyBlock( m_hDisk,
        m_VolumeStartSector.QuadPart + SourceLcn * m_BootSect.bpb.sectors_per_cluster,
        m_VolumeStartSector.QuadPart + DestLcn * m_BootSect.bpb.sectors_per_cluster,
        NumberOfLcns * m_BootSect.bpb.sectors_per_cluster);

    return bOk;
}

LONGLONG CNtfsController::GetNumberOfVcnsInDataRuns( LPBYTE DataRuns,DWORD Length )
/*++
功能描述:获取Dataruns描述数据的簇数量

参数:
    DataRuns:NTFS中的DataRuns结构
    Length:DataRuns的数组长度(不一定是dataruns的实际长度）

返回值:成功返回对应的簇数量,失败返回0
--*/
{
    LONGLONG  len = 0,totalLen = 0;

    for(DWORD i = 0;
        i < Length;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( DataRuns[i] == 0 )break;

        cStartLcnBytes = ( DataRuns[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (DataRuns[i] & 0x0f);        //压缩字节的低4位

        if( cStartLcnBytes != 0 )
        {
            //提取当前数据流长度
            len = 0;
            for( DWORD j = cLenLcnBytes;j > 0;j--)
            {
                len = (len << 8) | DataRuns[i + j ]; 
            }
            totalLen += len;
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }

    return totalLen;
}

LONGLONG CNtfsController::GetLastStartLcnInDataruns( LPBYTE DataRuns,DWORD Length )
/*++
功能描述:获取Dataruns数组描述的最后一个有效数据块的起始LCN

参数:
    DataRuns:NTFS中的DataRuns结构
    Length:DataRuns的数组长度(不一定是dataruns的实际长度）

返回值:成功返回对应的LCN，失败返回-1

--*/
{
    LONGLONG startLcn = 0;

    if( DataRuns == NULL)return -1;

    for(DWORD i = 0;
        i < Length;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( DataRuns[i] == 0 )break;

        cStartLcnBytes = ( DataRuns[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (DataRuns[i] & 0x0f);        //压缩字节的低4位

        if( cStartLcnBytes != 0)
        {
            //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
            LONGLONG tmp = 0;
            if( DataRuns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                tmp = -1ll;
            for( DWORD j = cStartLcnBytes;j > 0;j-- )
            {
                tmp = ( tmp << 8 ) | DataRuns[i + cLenLcnBytes + j ];
            }
            startLcn = startLcn + tmp;
            assert( startLcn >= 0 );
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }

    return startLcn<0?-1:startLcn;
}

DWORD    CNtfsController::GetDataRunsLength( IN LPBYTE DataRuns,DWORD Length )
/*++
功能描述:获取Dataruns数组的有效长度（不包含结尾的一个0字节）

参数:
    DataRuns:NTFS中的DataRuns结构
    Length:DataRuns的数组长度(不一定是dataruns的实际长度）

返回值:成功返回对应的字节数,失败返回0
--*/
{
    DWORD i = 0;
    for(i = 0;
        i < Length;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( DataRuns[i] == 0 )break;

        cStartLcnBytes = ( DataRuns[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (DataRuns[i] & 0x0f);        //压缩字节的低4位

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }

    return i;

}

LONGLONG CNtfsController::GetDataRunsValue(
    IN LPBYTE DataRuns,
    IN DWORD Length,
    OUT LPVOID Buffer,
    IN LONGLONG BufferLength )
/*++
功能描述:从Dataruns中提取真正的数据。

参数:
    DataRuns:NTFS中的DataRuns结构
    Length:DataRuns的数组长度(不一定是dataruns的实际长度）
    Buffer:输出缓冲区，用于接收数据
    BufferLength：输出缓冲区长度

返回值:成功返回0,失败返回 -1,如果Buffer为空,返回所需最少的Buffer长度（大于0)

--*/
{
    LONGLONG vcns = GetNumberOfVcnsInDataRuns( DataRuns,Length );
    assert( vcns > 0 );
    if( Buffer == NULL)
        return vcns * m_ClusterSizeInBytes;

    if( BufferLength < vcns * m_ClusterSizeInBytes )
    {
        DbgPrint(" buffer length is too small!,will return -1");
        return -1;
    }

    BOOL bOk = TRUE;
    for( LONGLONG i = 0;i < vcns;i++)
    {
        LONGLONG lcn = VcnToLcn( i,DataRuns,Length );
        bOk = ReadLogicalCluster( (LPBYTE)Buffer + i * m_ClusterSizeInBytes,
                            m_ClusterSizeInBytes,
                            lcn );
        if( !bOk )break;
    }

    return bOk?0:-1;
}

LONGLONG CNtfsController::VcnToLcn( LONGLONG Vcn,LPBYTE DataRuns,DWORD Length )
/*++
功能描述:虚拟扇区号转换为逻辑扇区号

参数:
    Vcn:虚拟扇区号
    DataRuns:NTFS中的DataRuns结构
    Length:DataRuns的数组长度(不一定是dataruns的实际长度）

返回值:转换成功返回Vcn对应的Lcn,否则返回 -1

--*/
{
    LONGLONG startLcn = 0,len = 0,totalLen = 0,result = -1;

    if( DataRuns == NULL)return -1;

    for(DWORD i = 0;
        i < Length;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( DataRuns[i] == 0 )break;

        cStartLcnBytes = ( DataRuns[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (DataRuns[i] & 0x0f);        //压缩字节的低4位

        if( cStartLcnBytes != 0)
        {
            //提取当前数据流长度
            len = 0;
            for( DWORD j = cLenLcnBytes;j > 0;j--)
            {
                len = (len << 8) | DataRuns[i + j ]; 
            }

            //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
            LONGLONG tmp = 0;
            if( DataRuns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                tmp = -1ll;
            for( DWORD j = cStartLcnBytes;j > 0;j-- )
            {
                tmp = ( tmp << 8 ) | DataRuns[i + cLenLcnBytes + j ];
            }
            startLcn = startLcn + tmp;
            assert( startLcn >= 0 );

            if( Vcn >= totalLen && Vcn < totalLen + len )
            {
                result = Vcn - totalLen + startLcn;
                break;
            }
            totalLen += len;
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }

    return result;
}

LONG CNtfsController::BlockListToDataRuns(PLIST_ENTRY BlockListHead, LPBYTE Dataruns, LONGLONG DatarunsLength)
/*++
功能描述：将用BlockList描述的区域转换为相应的Dataruns数据

参数：
    BlockListHead：块信息链表头结点指针
    Dataruns:输出缓冲区指针，用于接收转换完成的Dataruns
    DatarunsLength:指出输出缓冲区的长度

返回值:成功返回0,失败返回 -1,如果Dataruns为NULL,返回所需Dataruns长度（大于0)

--*/
{
    LONG length = 0;
    DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0,i = 0;
    LONGLONG startLcn = 0,lenLcn = 0,prevLcn = 0;

    for( PLIST_ENTRY list = BlockListHead->Flink;
        list != BlockListHead;
        list = list->Flink)
    {
        PBLOCK_DESCRIPTOR block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                    BLOCK_DESCRIPTOR,
                                                                    List);
        assert( block != NULL);

        BYTE    sectors_per_cluster = m_BootSect.bpb.sectors_per_cluster;
        LONGLONG startCluster = block->StartSector.QuadPart / sectors_per_cluster;
        LONGLONG lenInCluster = block->TotalSectors.QuadPart % sectors_per_cluster==0?
            block->TotalSectors.QuadPart / sectors_per_cluster:block->TotalSectors.QuadPart/sectors_per_cluster+1;

        startLcn = startCluster - prevLcn;
        prevLcn = startCluster;
        lenLcn = lenInCluster;
        assert( lenLcn > 0);

        //计算描述startLcn所需最少的字节数
        cStartLcnBytes = CompressLongLong( startLcn );

        //计算描述lenLcn所需最少的字节数 
        cLenLcnBytes = CompressLongLong( lenLcn );

        length += cStartLcnBytes + cLenLcnBytes + 1;
    }
    length++;
    if( length % 8 != 0 )length = (length / 8 + 1)*8;

    if( Dataruns == NULL)
        return length;
    if( DatarunsLength < length )
        return -1;


    //后续填充缓冲区:-)
    cStartLcnBytes = 0,cLenLcnBytes = 0,i = 0;
    startLcn = 0,lenLcn = 0,prevLcn = 0;
    for( PLIST_ENTRY list = BlockListHead->Flink;
        list != BlockListHead;
        list = list->Flink)
    {
        PBLOCK_DESCRIPTOR block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                    BLOCK_DESCRIPTOR,
                                                                    List);
        assert( block != NULL);

        BYTE    sectors_per_cluster = m_BootSect.bpb.sectors_per_cluster;
        LONGLONG startCluster = block->StartSector.QuadPart / sectors_per_cluster;
        LONGLONG lenInCluster = block->TotalSectors.QuadPart % sectors_per_cluster==0?
            block->TotalSectors.QuadPart / sectors_per_cluster:block->TotalSectors.QuadPart/sectors_per_cluster+1;

        startLcn = startCluster - prevLcn;
        prevLcn = startCluster;
        lenLcn = lenInCluster;
        assert( lenLcn > 0 );

        //计算描述startLcn所需最少的字节数
        cStartLcnBytes = CompressLongLong( startLcn );

        //计算描述lenLcn所需最少的字节数 
        cLenLcnBytes = CompressLongLong( lenLcn );

        Dataruns[i++] = (BYTE)((cStartLcnBytes << 4) | cLenLcnBytes);
        LONGLONG tmp = lenLcn;
        for( BYTE j = 0;j < cLenLcnBytes;j++)
        {
            Dataruns[i++] = (BYTE)(tmp & 0xff);
            tmp >>= 8;
        }
        tmp = startLcn;
        for( BYTE j = 0;j < cStartLcnBytes;j++)
        {
            Dataruns[i++] = (BYTE)(tmp & 0xff);
            tmp >>= 8;
        }
    }
    Dataruns[i++] = 0;

    return 0;
}

LONG CNtfsController::DataRunsToSpaceDataRuns(LPBYTE dataruns, LONGLONG len_dataruns, LPBYTE bad_dataruns, LONGLONG len_bad_dataruns)
/*++
功能描述：将常规Dataruns数据转换为用来描述空间信息的dataruns,
           ！！！！并在Bitmap中标记相关区域为占用

参数：
    dataruns：常规dataruns的缓冲区指针
    len_dataruns:参数dataruns描述的缓冲区长度（字节）
    bad_dataruns:输出缓冲区指针，用于接收转换完成的dataruns
    len_bad_dataruns:指出输出缓冲区的长度

返回值:成功返回0,失败返回 -1,如果bad_dataruns为NULL,返回所需dataruns长度（大于0)

--*/
{
    
    //计算转换完成需要的缓冲区长度
    LONG lengthRequired = 0;
    LONGLONG startLcn = 0,len = 0,prev_len = 0,curr_len = 0;
    BYTE cCurrLenBytes = 0;

    for(LONG i = 0;
        i < len_dataruns;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( dataruns[i] == 0 )break;

        cStartLcnBytes = ( dataruns[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (dataruns[i] & 0x0f);        //压缩字节的低4位

        if( cStartLcnBytes != 0)
        {
            //提取当前数据流长度
            len = 0;
            for( DWORD j = cLenLcnBytes;j > 0;j--)
            {
                len = (len << 8) | dataruns[i + j ]; 
            }

            //提取当前数据流起始簇号（是相对上一个数据流的偏移,有符号）
            LONGLONG tmp = 0;
            if( dataruns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                tmp = -1ll;
            for( DWORD j = cStartLcnBytes;j > 0;j-- )
            {
                tmp = ( tmp << 8 ) | dataruns[i + cLenLcnBytes + j ];
            }
            startLcn = startLcn + tmp;
            
            curr_len = tmp - prev_len;
            prev_len = len;
            cCurrLenBytes = CompressLongLong( curr_len );
            if( curr_len < 0 )cCurrLenBytes--;
            //bad_dataruns[len_bad_dataruns++]=cCurrLenBytes;
            lengthRequired++;
            //memcpy_s( bad_dataruns+len_bad_dataruns,m_MftRecordLength,&curr_len,cCurrLenBytes);
            //len_bad_dataruns += cCurrLenBytes;
            lengthRequired += cCurrLenBytes;
            //memcpy_s( bad_dataruns+len_bad_dataruns,
            //    m_MftRecordLength,
            //    &dataruns[i],
            //    cStartLcnBytes + cLenLcnBytes + 1);
            //len_bad_dataruns += cStartLcnBytes + cLenLcnBytes + 1;
            lengthRequired += cStartLcnBytes + cLenLcnBytes + 1;
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }

    curr_len = /*m_VolumeTotalSectors.QuadPart*/m_BootSect.number_of_sectors / m_BootSect.bpb.sectors_per_cluster
                - startLcn
                - prev_len;
    cCurrLenBytes = CompressLongLong( curr_len );
    if( curr_len < 0 )cCurrLenBytes--;

    //bad_dataruns[len_bad_dataruns++]=cCurrLenBytes;
    lengthRequired++;
    //memcpy_s( bad_dataruns+len_bad_dataruns,m_MftRecordLength,&curr_len,cCurrLenBytes);
    //len_bad_dataruns += cCurrLenBytes;
    lengthRequired += cCurrLenBytes;

    //bad_dataruns[len_bad_dataruns++] = 0;
    //len_bad_dataruns++;
    lengthRequired++;
    if( lengthRequired % 8 != 0 )
        lengthRequired = (lengthRequired / 8 + 1)*8;

    if( bad_dataruns == NULL)
        return lengthRequired;
    else if( len_bad_dataruns < lengthRequired )
        return -1;

    //填充输出缓冲区

    startLcn = 0,len = 0,prev_len = 0,curr_len = 0;
    cCurrLenBytes = 0;
    len_bad_dataruns = 0;

    for(LONG i = 0;
        i < len_dataruns;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( dataruns[i] == 0 )break;

        cStartLcnBytes = ( dataruns[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (dataruns[i] & 0x0f);        //压缩字节的低4位

        if( cStartLcnBytes != 0)
        {
            //提取当前数据流长度
            len = 0;
            for( DWORD j = cLenLcnBytes;j > 0;j--)
            {
                len = (len << 8) | dataruns[i + j ]; 
            }

            //提取当前数据流起始簇号（是相对上一个数据流的偏移,有符号）
            LONGLONG tmp = 0;
            if( dataruns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                tmp = -1ll;
            for( DWORD j = cStartLcnBytes;j > 0;j-- )
            {
                tmp = ( tmp << 8 ) | dataruns[i + cLenLcnBytes + j ];
            }
            startLcn = startLcn + tmp;
            
            curr_len = tmp - prev_len;
            prev_len = len;
            cCurrLenBytes = CompressLongLong( curr_len );
            bad_dataruns[len_bad_dataruns++]=cCurrLenBytes;
            memcpy_s( bad_dataruns+len_bad_dataruns,m_MftRecordLength,&curr_len,cCurrLenBytes);
            len_bad_dataruns += cCurrLenBytes;
            memcpy_s( bad_dataruns+len_bad_dataruns,
                m_MftRecordLength,
                &dataruns[i],
                cStartLcnBytes + cLenLcnBytes + 1);
            len_bad_dataruns += cStartLcnBytes + cLenLcnBytes + 1;
            
            //将Bitmap中相应的位标记为占用
            for(LONGLONG ii = startLcn;
                ii < startLcn + len;
                ii++)
            {
                m_Bitmap[ ii / 8 ] |= (1 << (ii % 8));
            }
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }

    curr_len = /*m_VolumeTotalSectors.QuadPart*/m_BootSect.number_of_sectors / m_BootSect.bpb.sectors_per_cluster
                - startLcn
                - prev_len;
    cCurrLenBytes = CompressLongLong( curr_len );
    bad_dataruns[len_bad_dataruns++]=cCurrLenBytes;
    memcpy_s( bad_dataruns+len_bad_dataruns,m_MftRecordLength,&curr_len,cCurrLenBytes);
    len_bad_dataruns += cCurrLenBytes;
    bad_dataruns[len_bad_dataruns++] = 0;

    return 0;
}

BOOL CNtfsController::InitMftFile( IN LPVOID MftRecordCluster,IN DWORD BufferLength )
/*++
功能描述:初始化$MFT文件的属性信息

参数:
    MftRecordCluster:指向包含$MFT文件记录的簇缓冲区
    BufferLength:指出缓冲区大小

返回值:成功返回TRUE并调整类成员变量m_MftDataRuns和m_MftDataRunsLength.
        失败返回FALSE

--*/
{
    PFILE_INFORMATION   pFileInfor = InitNtfsFile( MftRecordCluster,BufferLength,FILE_MFT );
    if( pFileInfor == NULL)return FALSE;
    pFileInfor->FileRecordId = FILE_MFT;

    //初始化用于寻址$MFT文件的Dataruns结构
    PLIST_ENTRY list;
    for( list = pFileInfor->List.Flink;
        list != &pFileInfor->List;
        list = list->Flink )
    {
        if(((PFILE_ATTRIBUTE_NODE)CONTAINING_RECORD( list,FILE_ATTRIBUTE_NODE,List ))
            ->AttributeType == AT_DATA ) break;
    }
    if( list == &pFileInfor->List )return FALSE;

    PATTR_RECORD pAttrRecord = (PATTR_RECORD)((PFILE_ATTRIBUTE_NODE)CONTAINING_RECORD( list,FILE_ATTRIBUTE_NODE,List ))
        ->AttributeData;
    assert( pAttrRecord->non_resident == 1 );
    m_MftDataRunsLength = pAttrRecord->length - pAttrRecord->mapping_pairs_offset;
    assert( m_MftDataRuns == NULL );
    m_MftDataRuns = (LPBYTE)malloc( m_MftDataRunsLength );
    assert( m_MftDataRuns != NULL );
    memcpy_s( m_MftDataRuns,m_MftDataRunsLength,
        (LPVOID)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset),
        m_MftDataRunsLength );

    //设置$MFT文件中文件记录数
    m_MftNumberOfRecord = pFileInfor->FileSize / m_MftRecordLength;
    CloseFile( pFileInfor );
    pFileInfor = NULL;

    return TRUE;

}

BOOL CNtfsController::InitBitmap()
/*++
功能描述:初始化Bitmap,用于后续处理空间统计,分配等操作

参数:无

返回值:成功返回TRUE,并初始化类成员变量m_Bitmap和m_BitmapLength
        失败返回FALSE
--*/
{
    BOOL bResult = TRUE;

    NTFS_FILE file = OpenNtfsFile( FILE_Bitmap );
    if( file == NULL)
    {
        bResult = FALSE;
        goto exit;
    }

    LONG length = GetAttributeValue( file,AT_DATA,NULL,0 );
    if( length <= 0 )
    {
        bResult = FALSE;
        goto exit;
    }

    LPVOID buffer = malloc( length );
    assert( buffer != NULL );
    BOOL bDataruns = FALSE;
    if( 0 != GetAttributeValue( file,AT_DATA,buffer,length,&bDataruns ))
    {
        bResult = FALSE;
        goto exit;
    }
    assert( bDataruns == TRUE );

    LONGLONG vcns = GetNumberOfVcnsInDataRuns( (LPBYTE)buffer,length );
    if( vcns <= 0 )
    {
        bResult = FALSE;
        goto exit;
    }

    m_Bitmap = (LPBYTE)malloc( (size_t)(vcns * m_ClusterSizeInBytes));
    assert( m_Bitmap != NULL );
    RtlZeroMemory( m_Bitmap,(size_t)(vcns * m_ClusterSizeInBytes));

    for( LONGLONG i = 0;i < vcns;i++)
    {
        LONGLONG lcn = VcnToLcn( i,(LPBYTE)buffer,length );
        if( lcn == -1 ){
            bResult = FALSE;
            free( m_Bitmap );
            m_Bitmap = NULL;
            goto exit;
        }

        bResult = ReadLogicalCluster( m_Bitmap + i * m_ClusterSizeInBytes,m_ClusterSizeInBytes,lcn );
        if( !bResult )
        {
            free( m_Bitmap );
            m_Bitmap = NULL;
            goto exit;
        }
    }

    m_BitmapLength = file->FileSize;//这是一个按8字节对齐的值

exit:
    CloseFile( file );
    file = NULL;

    if( buffer != NULL){
        free(buffer );
        buffer = NULL;
    }

    return bResult;
}

BOOL CNtfsController::InitMftBitmap()
/*++
功能描述:初始化MftBitmap,用于后续处理文件记录块的分配回收等。

参数:无

返回值:成功返回TRUE,并初始化类成员变量m_MftBitmap和m_MftBitmapLength
        失败返回FALSE
--*/
{
    BOOL bResult = TRUE,bDataruns = FALSE;;
    LPBYTE dataruns = NULL;
    LONG len_dataruns = 0;

    NTFS_FILE hMft = OpenNtfsFile( FILE_MFT );
    if( hMft == NULL)
    {
        bResult = FALSE;
        DbgPrint("open mft file failed!");
        ReportErrorMessage("主文件表打开失败!   ");
        goto exit;
    }

    len_dataruns = GetAttributeValue( hMft,AT_BITMAP,NULL,0,&bDataruns);
    assert( len_dataruns > 0 );
    assert( bDataruns );
    dataruns = (LPBYTE)malloc( len_dataruns );
    assert( dataruns > 0 );

    if( 0 != GetAttributeValue( hMft,AT_BITMAP,dataruns,len_dataruns ))
    {
        bResult = FALSE;
        DbgPrint("get mft bitmap failed!");
        ReportErrorMessage("提取文件记录位图失败!   ");
        free( dataruns );dataruns = NULL;
        len_dataruns = 0;
        CloseFile( hMft );hMft = NULL;
        goto exit;
    }

    m_MftBitmapLength = GetDataRunsValue( dataruns,len_dataruns,NULL,0);
    assert( m_MftBitmapLength > 0 );
    m_MftBitmap = (LPBYTE)malloc( (DWORD)m_MftBitmapLength );
    assert( m_MftBitmap != NULL);
    GetDataRunsValue( dataruns,len_dataruns,m_MftBitmap,m_MftBitmapLength);
    
exit:
    if( hMft != NULL)
    {
        CloseFile( hMft );
        hMft = NULL;
    }

    if( dataruns != NULL)
    {
        free( dataruns );
        dataruns = NULL;
    }

    return bResult;
}

BOOL CNtfsController::UpdateBitmap()
/*++
功能描述：更新NTFS磁盘空间位图（将更改过的位图写入磁盘中）

参数：无

返回值：成功返回TRUE，失败返回FALSE
--*/
{
    BOOL bResult = TRUE;

    NTFS_FILE file = OpenNtfsFile( FILE_Bitmap );
    if( file == NULL)
    {
        bResult = FALSE;
        goto exit;
    }

    LONG length = GetAttributeValue( file,AT_DATA,NULL,0 );
    if( length <= 0 )
    {
        bResult = FALSE;
        goto exit;
    }

    LPVOID buffer = malloc( length );
    assert( buffer != NULL );
    BOOL bDataruns = FALSE;
    if( 0 != GetAttributeValue( file,AT_DATA,buffer,length,&bDataruns ))
    {
        bResult = FALSE;
        goto exit;
    }
    assert( bDataruns == TRUE );

    LONGLONG vcns = GetNumberOfVcnsInDataRuns( (LPBYTE)buffer,length );
    if( vcns <= 0 )
    {
        bResult = FALSE;
        goto exit;
    }

    assert( m_Bitmap != NULL && m_BitmapLength > 0 && m_BitmapLength <= vcns * m_ClusterSizeInBytes );

    for( LONGLONG i = 0;i < vcns;i++)
    {
        LONGLONG lcn = VcnToLcn( i,(LPBYTE)buffer,length );
        if( lcn == -1 ){
            bResult = FALSE;
            goto exit;
        }

        bResult = WriteLogicalCluster( m_Bitmap + i * m_ClusterSizeInBytes,m_ClusterSizeInBytes,lcn );
        if( !bResult )
        {
            goto exit;
        }
    }

exit:
    CloseFile( file );
    file = NULL;

    if( buffer != NULL){
        free(buffer );
        buffer = NULL;
    }

    return bResult;

}

BOOL CNtfsController::UpdateMftBitmap()
/*++
功能描述：更新NTFS文件记录块分配位图（将更改过的位图写入磁盘中）

参数：无

返回值：成功返回TRUE，失败返回FALSE
--*/
{
    BOOL bResult = TRUE;

    NTFS_FILE file = OpenNtfsFile( FILE_MFT );
    if( file == NULL)
    {
        bResult = FALSE;
        goto exit;
    }

    LONG length = GetAttributeValue( file,AT_BITMAP,NULL,0 );
    if( length <= 0 )
    {
        bResult = FALSE;
        goto exit;
    }

    LPVOID buffer = malloc( length );
    assert( buffer != NULL );
    BOOL bDataruns = FALSE;
    if( 0 != GetAttributeValue( file,AT_BITMAP,buffer,length,&bDataruns ))
    {
        bResult = FALSE;
        goto exit;
    }
    assert( bDataruns == TRUE );

    LONGLONG vcns = GetNumberOfVcnsInDataRuns( (LPBYTE)buffer,length );
    if( vcns <= 0 )
    {
        bResult = FALSE;
        goto exit;
    }

    assert( m_MftBitmap != NULL && m_MftBitmapLength > 0 && m_MftBitmapLength <= vcns * m_ClusterSizeInBytes );

    for( LONGLONG i = 0;i < vcns;i++)
    {
        LONGLONG lcn = VcnToLcn( i,(LPBYTE)buffer,length );
        if( lcn == -1 ){
            bResult = FALSE;
            goto exit;
        }

        bResult = WriteLogicalCluster( m_MftBitmap + i * m_ClusterSizeInBytes,m_ClusterSizeInBytes,lcn );
        if( !bResult )
        {
            goto exit;
        }
    }

exit:
    CloseFile( file );
    file = NULL;

    if( buffer != NULL){
        free(buffer );
        buffer = NULL;
    }

    return bResult;

}

BOOL CNtfsController::UpdateBadClus()
/*++
功能描述：更新NTFS磁盘坏块文件$BadClus（将更改过的坏簇描述信息写入磁盘中）

参数：无

返回值：成功返回TRUE，失败返回FALSE
--*/
{
    LPBYTE dataruns = NULL,bad_dataruns = NULL;
    LONG len_dataruns = 0,len_bad_dataruns = 0;

    len_dataruns = BlockListToDataRuns( &m_BlockInforHead.BadBlockList,NULL,0);
    assert( len_dataruns != 0 );

    if( len_dataruns < 0 )
    {
        DbgPrint("len_dataruns is -1");
        goto error_exit;
    }

    dataruns = (LPBYTE)malloc( len_dataruns );
    assert( dataruns != NULL);
    RtlZeroMemory( dataruns,len_dataruns );
    LONG result = BlockListToDataRuns( &this->m_BlockInforHead.BadBlockList,
                                        dataruns,
                                        len_dataruns);
    if( result != 0 )
    {
        DbgPrint("get dataruns from blocklist failed!");
        goto error_exit;
    }

    //将常规dataruns转换为空间描述类型的dataruns

    len_bad_dataruns = DataRunsToSpaceDataRuns( dataruns,len_dataruns,NULL,0);
    assert( len_bad_dataruns > 0 );
    bad_dataruns = (LPBYTE)malloc( len_bad_dataruns );
    assert( bad_dataruns != NULL);
    RtlZeroMemory( bad_dataruns,len_bad_dataruns );
    if( -1 == DataRunsToSpaceDataRuns( dataruns,len_dataruns,bad_dataruns,len_bad_dataruns ))
    {
        ReportErrorMessage("dataruns to spacedataruns failed!");
        goto error_exit;
    }
    free( dataruns );dataruns = NULL;len_dataruns = 0;

    //将空间描述类型的dataruns,即bad_dataruns写入文件$BadClus的$Bad数据流中
    //认为$Bad数据流是$BadClus文件的最后一个属性

    //释放组成$BadClus文件的非基本文件记录
    NTFS_FILE hBadClusFile = OpenNtfsFile( FILE_BadClus );
    assert( hBadClusFile != NULL);
    for(PLIST_ENTRY list = hBadClusFile->List.Flink;
        list != &hBadClusFile->List;
        list = list->Flink)
    {
        PFILE_ATTRIBUTE_NODE node = (PFILE_ATTRIBUTE_NODE)
            CONTAINING_RECORD( list,
                               FILE_ATTRIBUTE_NODE,
                               List);
        if(node->OwnerRecordId != FILE_BadClus )
            FreeFileRecordId( node->OwnerRecordId );
    }
    CloseFile( hBadClusFile );
    hBadClusFile = NULL;

    //重建$BadClus文件的文件记录
    
    LPBYTE buffer = NULL;
    buffer = (LPBYTE)malloc( this->m_MftRecordLength );
    assert( buffer != NULL);
    BOOL bOk = ReadMftRecord( FILE_BadClus,buffer,m_MftRecordLength );
    assert( bOk );
    PMFT_RECORD pRecordHeader = (PMFT_RECORD)buffer;
            
    PWORD   pUsa = (PWORD)((DWORD_PTR)pRecordHeader + pRecordHeader->usa_ofs);
    WORD    cUsa = pRecordHeader->usa_count;

    //用USA数组更新扇区末尾两个字节的数据
    for( WORD i = 1;i < cUsa;i++)
    {
        assert( *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) == pUsa[0]);
        *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[i];
    }

    PATTR_RECORD pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pRecordHeader + pRecordHeader->attrs_offset);
    assert(pAttrRecord->type == AT_STANDARD_INFORMATION);
    pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
    if( pAttrRecord->type == AT_ATTRIBUTE_LIST )
    {
        FreeBlockInDataruns( (LPBYTE)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset),
            pAttrRecord->length - pAttrRecord->mapping_pairs_offset);
        PATTR_RECORD pAttrRecord2 = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
        assert( pAttrRecord2->type == AT_FILE_NAME );
        pAttrRecord2 = (PATTR_RECORD)((DWORD_PTR)pAttrRecord2 + pAttrRecord2->length);
        assert( pAttrRecord2->type == AT_DATA && pAttrRecord2->name_length == 0 );
        pAttrRecord2 = (PATTR_RECORD)((DWORD_PTR)pAttrRecord2 + pAttrRecord2->length);
        assert( pAttrRecord2->type == AT_DATA && pAttrRecord2->name_length == 4 );
        pAttrRecord2 = (PATTR_RECORD)((DWORD_PTR)pAttrRecord2 + pAttrRecord2->length);
        assert( pAttrRecord2->type == AT_END );
        RtlMoveMemory( pAttrRecord,
                (LPVOID)((DWORD_PTR)pAttrRecord + pAttrRecord->length),
                (DWORD_PTR)pAttrRecord2 + 8 - ((DWORD_PTR)pAttrRecord + pAttrRecord->length));
    }

    assert( pAttrRecord->type == AT_FILE_NAME);
    pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
    assert( pAttrRecord->type == AT_DATA && pAttrRecord->name_length == 0);
    pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
    assert( pAttrRecord->type == AT_DATA && pAttrRecord->name_length == 4);
    assert(pAttrRecord->non_resident == 1);
    PATTR_RECORD pAttrRecord2 = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
    assert( pAttrRecord2->type == AT_END );

    LONG bytesCanBeUse = (LONG)
        (((DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated - 8)
        - ((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset));
    if( bytesCanBeUse >= len_bad_dataruns )
    {  
        //一个文件记录空间够用的情况

        memcpy_s( (LPBYTE)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset),
                   bytesCanBeUse,
                   bad_dataruns,
                   len_bad_dataruns );
        pAttrRecord->length = pAttrRecord->mapping_pairs_offset + (DWORD)len_bad_dataruns;
        pAttrRecord->lowest_vcn = 0;
        pAttrRecord->highest_vcn = 
            m_BootSect.number_of_sectors / m_BootSect.bpb.sectors_per_cluster -1;
        *(PDWORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length) = 0xffffffff;
        pRecordHeader->bytes_in_use = (DWORD)(((DWORD_PTR)pAttrRecord + pAttrRecord->length)
                                        - (DWORD_PTR)pRecordHeader
                                        + 8);

        //更新USA数组及扇区末尾两个字节的数据
        for( WORD i = 1;i < cUsa;i++)
        {
            pUsa[i] = *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD));
            *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[0];
        }

        bOk = WriteMftRecord( FILE_BadClus,buffer,m_MftRecordLength );
        if(!bOk )
        {
            DbgPrint("write mftrecord failed!");
            goto error_exit;
        }
    }
    else
    {
        //单个文件记录空间不够用，需要添加ATTRIBUTE_LIST属性及执行相关操作
        
        //构建基本文件记录中对应的ATTRIBUTE_LIST_ENTRY项
        LPBYTE attributeListData = NULL;
        //LONG len_attributeListData = 3 * (sizeof(ATTR_LIST_ENTRY) + 6)+sizeof(ATTR_LIST_ENTRY)+4*sizeof(WCHAR)+6;
        LONG len_attributeListData = 4 * (sizeof(ATTR_LIST_ENTRY) + 6)+4*sizeof(WCHAR);
        attributeListData = (LPBYTE)malloc( len_attributeListData );
        assert( attributeListData != NULL);
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pRecordHeader + pRecordHeader->attrs_offset);
        PATTR_LIST_ENTRY p = (PATTR_LIST_ENTRY)attributeListData;
        int count = 0;
        for( ;pAttrRecord->type != AT_END && !(pAttrRecord->type == AT_DATA && pAttrRecord->name_length == 4);
            pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length))
        {
            p->type = pAttrRecord->type;
            p->length = sizeof( ATTR_LIST_ENTRY ) + 6;//无属性名，对齐到8字节边界
            p->lowest_vcn = 0;
            p->mft_reference = MK_MREF( FILE_BadClus,pRecordHeader->sequence_number);
            p->name_length = 0;
            p->name_offset = sizeof(ATTR_LIST_ENTRY );
            p->instance = pAttrRecord->instance;
            
            p = (PATTR_LIST_ENTRY)((DWORD_PTR)p + p->length);
            count++;
        }
        assert( count == 3 );
        assert( pAttrRecord->type == AT_DATA && pAttrRecord->name_length == 4);
        p->type = pAttrRecord->type;
        p->length = sizeof( ATTR_LIST_ENTRY)+4*sizeof(WCHAR)+6;
        p->lowest_vcn = 0;
        p->mft_reference = MK_MREF( FILE_BadClus,pRecordHeader->sequence_number);
        p->name_length = 4;
        p->name_offset = sizeof( ATTR_LIST_ENTRY );
        p->instance = pAttrRecord->instance;
        memcpy_s( p->name,
                4*sizeof(WCHAR),
                (LPVOID)((DWORD_PTR)pAttrRecord + pAttrRecord->name_offset),
                4*sizeof(WCHAR));

        //调整基本文件记录

        //从AT_FILE_NAME属性开始的所有属性向后移动，为ATTRIBUTE_LIST属性保留空间
        PATTR_RECORD tmp = pAttrRecord;
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pRecordHeader + pRecordHeader->attrs_offset);
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
        assert( pAttrRecord->type == AT_FILE_NAME );
        RtlMoveMemory((LPVOID)((DWORD_PTR)pAttrRecord + sizeof(ATTR_RECORD)+0x10),//0x10是为dataruns保留的空间大小
            pAttrRecord,
            (DWORD_PTR)tmp +sizeof(ATTR_RECORD) + 4*sizeof(WCHAR) - (DWORD_PTR)pAttrRecord);

        //填充ATTRIBUTE_LIST的属性头信息
        pAttrRecord->type = AT_ATTRIBUTE_LIST;
        pAttrRecord->length = sizeof(ATTR_RECORD)+0x10;
        pAttrRecord->non_resident = 1;
        pAttrRecord->name_length = 0;
        pAttrRecord->name_offset = sizeof(ATTR_RECORD);
        pAttrRecord->flags = ATTR_NORMAL;
        pAttrRecord->instance = pRecordHeader->next_attr_instance++;
        pAttrRecord->lowest_vcn
            = pAttrRecord->highest_vcn = 0;
        pAttrRecord->mapping_pairs_offset = sizeof( ATTR_RECORD );
        pAttrRecord->compression_unit = 0;
        pAttrRecord->allocated_size = m_ClusterSizeInBytes;//一个簇
        pAttrRecord->data_size = 
            pAttrRecord->initialized_size =0;//后边ATTR_LIST的内容确定后再赋值

        LONGLONG newBlock = AllocateBlock( 1 );
        LPBYTE pDataruns = (LPBYTE)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset);
        assert( newBlock > 0 );
        BYTE len_newBlock = CompressLongLong( newBlock );
        pDataruns[0] = (len_newBlock << 4) | 0x01;
        pDataruns[1] = 1;
        memcpy_s( &pDataruns[2],
                  16 -2 -1,
                  &newBlock,
                  len_newBlock );
        pDataruns[len_newBlock + 2]=0;//ATTR_LIST属性的dataruns赋值完成

        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
        assert( pAttrRecord->type == AT_FILE_NAME );
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
        assert( pAttrRecord->type == AT_DATA && pAttrRecord->name_length==0 );
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
        assert( pAttrRecord->type == AT_DATA && pAttrRecord->name_length==4 );

        pDataruns = (LPBYTE)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset);
        DWORDLONG curr_vcn = 0,len = 0;
        pAttrRecord->lowest_vcn = 0;
        LONGLONG curr_rec_id = FILE_BadClus;
        WORD baseSeq = pRecordHeader->sequence_number;
        for( LONG i = 0;i < len_bad_dataruns;)
        {
            DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

            if( bad_dataruns[i] == 0 )break;

            cStartLcnBytes = ( bad_dataruns[i] & 0xf0) >> 4;//压缩字节的高4位
            cLenLcnBytes = (bad_dataruns[i] & 0x0f);        //压缩字节的低4位

            //提取当前数据流长度
            len = 0;
            for( DWORD j = cLenLcnBytes;j > 0;j--)
            {
                len = (len << 8) | bad_dataruns[i + j ]; 
            }

            if(  (cStartLcnBytes + cLenLcnBytes + 1 ) >
                ((DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated-8-1-(DWORD_PTR)pDataruns))
            {
                //当前文件记录空间已经用完，需要增加文件记录，并保存当前文件记录
                *pDataruns = 0;
                pAttrRecord->highest_vcn = curr_vcn - 1;
                pAttrRecord->length = (DWORD)((DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated - 8
                                      - (DWORD_PTR)pAttrRecord);
                *(PLONG)((DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated-8)
                    = AT_END;
                pAttrRecord->allocated_size =
                    pAttrRecord->data_size = 
                    pAttrRecord->initialized_size = 0;
                pRecordHeader->bytes_in_use = pRecordHeader->bytes_allocated;

                //将当前文件记录写入磁盘

                //更新USA数组及扇区末尾两个字节的数据
                for( WORD j = 1;j < cUsa;j++)
                {
                    pUsa[j] = *(PWORD)((DWORD_PTR)pRecordHeader + j * m_BootSect.bpb.bytes_per_sector - sizeof(WORD));
                    *(PWORD)((DWORD_PTR)pRecordHeader + j * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[0];
                }

                bOk = WriteMftRecord( curr_rec_id,pRecordHeader,m_MftRecordLength );
                if(!bOk )
                {
                    DbgPrint("write mftrecord failed!");
                    goto error_exit;
                }

                //申请新的文件记录
                curr_rec_id = AllocateFileRecordId();
                if( curr_rec_id < 0 )
                {
                    DbgPrint("allocate mftrecord failed!");
                    goto error_exit;
                }
                bOk = ReadMftRecord( curr_rec_id,buffer,m_MftRecordLength );
                assert( bOk );
                pRecordHeader = (PMFT_RECORD)buffer;
                //用USA数组更新扇区末尾两个字节的数据
                for( WORD j = 1;j < cUsa;j++)
                {
                    assert( *(PWORD)((DWORD_PTR)pRecordHeader + j * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) == pUsa[0]);
                    *(PWORD)((DWORD_PTR)pRecordHeader + j * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[j];
                }

                pRecordHeader->flags = MFT_RECORD_IN_USE;
                pRecordHeader->link_count = 0;
                pRecordHeader->next_attr_instance = 0;
                pRecordHeader->base_mft_record = MK_MREF( FILE_BadClus,baseSeq );
                pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pRecordHeader + pRecordHeader->attrs_offset);
                pAttrRecord->type = AT_DATA;
                pAttrRecord->instance = pRecordHeader->next_attr_instance++;
                pAttrRecord->compression_unit = 0;
                pAttrRecord->flags = ATTR_NORMAL;
                pAttrRecord->lowest_vcn = curr_vcn;
                pAttrRecord->name_length = 4;
                pAttrRecord->name_offset = sizeof(ATTR_RECORD );
                pAttrRecord->non_resident = 1;
                memcpy_s( 
                    (LPVOID)((DWORD_PTR)pAttrRecord + pAttrRecord->name_offset),
                    8,
                    L"$Bad",
                    8);
                pAttrRecord->mapping_pairs_offset = sizeof(ATTR_RECORD)+8;
                pDataruns =(LPBYTE)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset);

                LONGLONG tmp_len = 0;
                if( cStartLcnBytes == 0 )
                {
                    curr_vcn += len;
                    memcpy_s( pDataruns,
                        (DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated-8-1-(DWORD_PTR)pDataruns,
                        bad_dataruns + i,
                        cStartLcnBytes + cLenLcnBytes + 1);
                    pDataruns += cStartLcnBytes + cLenLcnBytes + 1;
                    i += cStartLcnBytes + cLenLcnBytes + 1;
                    tmp_len = len;
                }
                if( bad_dataruns[i] != 0 )
                {
                    assert( (bad_dataruns[i] & 0xf0) != 0 );
                    cLenLcnBytes = bad_dataruns[i] & 0x0f;
                    len = 0;
                    for( DWORD j = cLenLcnBytes;j > 0;j--)
                    {
                        len = (len << 8) | bad_dataruns[i + j ]; 
                    }
                    LONGLONG curr_lcn = pAttrRecord->lowest_vcn + tmp_len;
                    cStartLcnBytes = CompressLongLong( curr_lcn );
                    *pDataruns++ = ((BYTE)cStartLcnBytes << 4 ) | (BYTE)cLenLcnBytes;
                    memcpy_s( pDataruns,
                        (DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated-8-1-(DWORD_PTR)pDataruns,
                        bad_dataruns + i+1,
                        cLenLcnBytes);
                    pDataruns += cLenLcnBytes;
                    memcpy_s( pDataruns,
                        (DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated-8-1-(DWORD_PTR)pDataruns,
                        &curr_lcn,
                        cStartLcnBytes);
                    pDataruns += cStartLcnBytes;
                    curr_vcn += len;
                    i += (bad_dataruns[i] & 0x0f) + 
                         ((bad_dataruns[i] & 0xf0) >> 4) + 1;
                }

                len_attributeListData += sizeof(ATTR_LIST_ENTRY)+6+4*sizeof(WCHAR);
                LPVOID p_xx = realloc( attributeListData,
                    len_attributeListData );
                assert( p_xx != NULL);
                attributeListData = (LPBYTE)p_xx;
                p = (PATTR_LIST_ENTRY)((DWORD_PTR)attributeListData+
                        len_attributeListData
                        - (sizeof(ATTR_LIST_ENTRY)+6+4*sizeof(WCHAR)));
                p->type = pAttrRecord->type;
                p->length = sizeof(ATTR_LIST_ENTRY)+6+4*sizeof(WCHAR);
                p->lowest_vcn = pAttrRecord->lowest_vcn;
                p->mft_reference = MK_MREF(curr_rec_id,pRecordHeader->sequence_number);
                p->name_length = 4;
                p->name_offset = sizeof(ATTR_LIST_ENTRY);
                p->instance = 0;
                memcpy_s( p->name,
                    4*sizeof(WCHAR),
                    L"$Bad",
                    4*sizeof(WCHAR));
                
                continue;
            }// end if 当前文件记录空间用完

            curr_vcn += len;
            memcpy_s( pDataruns,
                (DWORD_PTR)pRecordHeader + pRecordHeader->bytes_allocated-8-1-(DWORD_PTR)pDataruns,
                bad_dataruns + i,
                cStartLcnBytes + cLenLcnBytes + 1);
            pDataruns += cStartLcnBytes + cLenLcnBytes + 1;
            i += cStartLcnBytes + cLenLcnBytes + 1;
        }// end for i
        
        *pDataruns++ = 0;
        pAttrRecord->highest_vcn = curr_vcn - 1;
        while( (DWORD_PTR)pDataruns % 8 != 0)pDataruns++;
        *(PULONG)pDataruns = AT_END;
        pAttrRecord->length = (DWORD)( (DWORD_PTR)pDataruns - (DWORD_PTR)pAttrRecord);
        pAttrRecord->allocated_size =
            pAttrRecord->data_size = 
            pAttrRecord->initialized_size = 0;
        pRecordHeader->bytes_in_use = (DWORD)((DWORD_PTR)pDataruns + 8 - (DWORD_PTR)pRecordHeader);

        //将当前文件记录写入磁盘

        //更新USA数组及扇区末尾两个字节的数据
        for( WORD j = 1;j < cUsa;j++)
        {
            pUsa[j] = *(PWORD)((DWORD_PTR)pRecordHeader + j * m_BootSect.bpb.bytes_per_sector - sizeof(WORD));
            *(PWORD)((DWORD_PTR)pRecordHeader + j * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[0];
        }

        bOk = WriteMftRecord( curr_rec_id,pRecordHeader,m_MftRecordLength );
        if(!bOk )
        {
            DbgPrint("write mftrecord failed!");
            goto error_exit;
        }
        
        //ATTR_LIST的内容写入磁盘,簇号为newBlock
        bOk = WriteLogicalCluster(attributeListData
                ,len_attributeListData,
                newBlock);
        if( !bOk )
        {
            free( attributeListData );
            DbgPrint("write attr_list_data failed!");
            goto error_exit;
        }

        bOk = ReadMftRecord( FILE_BadClus,buffer,m_MftRecordLength );
        assert( bOk );
        //没必要检测USA和USN了
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pRecordHeader +pRecordHeader->attrs_offset);
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord +pAttrRecord->length);
        assert( pAttrRecord->type == AT_ATTRIBUTE_LIST );
        assert( pAttrRecord->data_size == 0 );
        pAttrRecord->initialized_size 
            = pAttrRecord->data_size = len_attributeListData;
        pAttrRecord->allocated_size = m_ClusterSizeInBytes * 1;
        
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord +pAttrRecord->length);
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord +pAttrRecord->length);
        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord +pAttrRecord->length);
        assert( pAttrRecord->type == AT_DATA && pAttrRecord->name_length == 4 );
        pAttrRecord->data_size = 
            pAttrRecord->allocated_size = curr_vcn * m_ClusterSizeInBytes;
        pAttrRecord->initialized_size = 0;
        bOk = WriteMftRecord( FILE_BadClus,buffer,m_MftRecordLength );
        assert( bOk );

        free( attributeListData );
        attributeListData = NULL;
        len_attributeListData = 0;

    }//else if byteCanBeUse

    free( buffer );buffer = NULL;
    free( bad_dataruns);bad_dataruns = NULL;len_bad_dataruns = 0;

    return TRUE;

error_exit:
    if( dataruns != NULL){
        free(dataruns);
        dataruns = NULL;
        len_dataruns = 0;
    }
    if( bad_dataruns != NULL){
        free( bad_dataruns);
        bad_dataruns = NULL;
        len_bad_dataruns = 0;
    }

    if( buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }

    return FALSE;

//快疯了，猥琐的代码！！！！！！！！！！！！！！！
}

BOOL CNtfsController::UpdateMftMirr()
/*++
功能描述：使$MFTMirr文件的内容与$Mft文件同步

参数：无

返回值：成功返回TRUE，失败返回FALSE
--*/
{
    LPBYTE buffer = NULL;
    DWORD  numberOfCluster = 0,i = 0;                 //$MFTMirr文件的簇数
    NTFS_FILE   hFile = OpenNtfsFile( FILE_MFTMirr );
    if( hFile == NULL)
        return FALSE;
    numberOfCluster = (DWORD)(hFile->FileSize / m_ClusterSizeInBytes);
    CloseFile( hFile );
    hFile = NULL;

    assert( numberOfCluster >= 1 );
    buffer = (LPBYTE)malloc( m_ClusterSizeInBytes );
    RtlZeroMemory( buffer,m_ClusterSizeInBytes );
    
    for( i = 0;i < numberOfCluster;i++)
    {
        BOOL bOk = ReadLogicalCluster( buffer,m_ClusterSizeInBytes,
            m_BootSect.mft_lcn + i);
        if( !bOk )
            break;
        bOk = WriteLogicalCluster(buffer,m_ClusterSizeInBytes,m_BootSect.mftmirr_lcn + i );
        if( !bOk )
            break;
    }

    free( buffer );buffer = NULL;
    if( i == numberOfCluster )
        return TRUE;
    else
        return FALSE;
}

BOOL CNtfsController::InitBadBlockList()
/*++
功能描述：从NTFS的元文件$Bad中初始化坏块信息链表

参数：无

返回值：成功返回TRUE，失败返回FALSE
--*/
{
    BOOL bResult = FALSE;
    LONGLONG last_value=0;                  //用于检查BAdBlockList是升序排序的断言

    NTFS_FILE   file = OpenNtfsFile( FILE_BadClus );
    if( file == NULL )
    {
        bResult = FALSE;
        goto exit;
    }

    DWORD valueLength = GetAttributeValue( file,AT_DATA,NULL,0,NULL,L"$Bad");
    if( valueLength == -1 )
    {
        bResult = FALSE;
        goto exit;
    }
    assert( valueLength > 0 );
    
    LPBYTE buffer = (LPBYTE)malloc( valueLength );
    BOOL bDataruns = FALSE;
    if( 0 != GetAttributeValue( file,AT_DATA,buffer,valueLength,&bDataruns,L"$Bad"))
    {
        bResult = FALSE;
        goto exit;
    }
    assert( bDataruns == TRUE );

    //从Dataruns中提取坏块信息

    DestroyListNodes( &m_BlockInforHead.BadBlockList);
    m_BlockInforHead.BadBlockSize.QuadPart = 0;

    LONGLONG startLcn = 0,len = 0;
    for(DWORD i = 0;
        i < valueLength;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( buffer[i] == 0 )break;

        cStartLcnBytes = ( buffer[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (buffer[i] & 0x0f);        //压缩字节的低4位

        //提取当前数据流长度
        len = 0;
        for( DWORD j = cLenLcnBytes;j > 0;j--)
        {
            len = (len << 8) | buffer[i + j ]; 
        }

        //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
        LONGLONG tmp = 0;
        if( buffer[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
            tmp = -1ll;
        for( DWORD j = cStartLcnBytes;j > 0;j-- )
        {
            tmp = ( tmp << 8 ) | buffer[i + cLenLcnBytes + j ];
        }
        startLcn = startLcn + tmp;
        if( cStartLcnBytes > 0 )
        {
            assert( startLcn > last_value );
            last_value = startLcn;
            AddBadBlock( startLcn * m_BootSect.bpb.sectors_per_cluster,
                        len * m_BootSect.bpb.sectors_per_cluster );
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }

    bResult = TRUE;

exit:
    if( buffer != NULL)
    {
        free( buffer );
        buffer = NULL;
    }
    
    if( file != NULL)
    {
        CloseFile( file );
        file = NULL;
    }

    return bResult;
}

PFILE_INFORMATION CNtfsController::InitNtfsFile( IN LPVOID MftRecordCluster,IN DWORD BufferLength,LONGLONG RecordId )
/*++
功能描述:初始化文件的属性信息

参数:
    MftRecordCluster:指向包含文件记录头的缓冲区
    BufferLength:指出缓冲区大小
    RecordId:文件记录ID

返回值:成功返回指向文件描述信息的指针（PFILE_INFORMATION),失败返回NULL

说明：之所以不用文件记录头中描述的文件记录ID是为了兼容低版本的NTFS文件系统
--*/
{
    PMFT_RECORD pRecordHeader = (PMFT_RECORD)MftRecordCluster;
    assert( pRecordHeader != NULL);

    //检查是否为文件记录
    if( !ntfs_is_file_record( pRecordHeader->magic ))
        return NULL;

    //检查文件记录是否在使用中
    if( !(pRecordHeader->flags & MFT_RECORD_IN_USE) )
        return NULL;

    //检查文件记录是否为基本文件记录
    if( (pRecordHeader->base_mft_record & 0x0000ffffffffffffull)!= 0 )
        return NULL;

    PWORD   pUsa = (PWORD)((DWORD_PTR)pRecordHeader + pRecordHeader->usa_ofs);
    WORD    cUsa = pRecordHeader->usa_count;

    //用USA数组更新扇区末尾两个字节的数据
    for( WORD i = 1;i < cUsa;i++)
    {
        assert( *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) == pUsa[0]);
        *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[i];
    }

    PFILE_INFORMATION   pFileInfor = (PFILE_INFORMATION )
        malloc( sizeof( FILE_INFORMATION ));
    assert( pFileInfor != NULL );
    RtlZeroMemory( pFileInfor,sizeof( FILE_INFORMATION ));
    InitializeListHead( &pFileInfor->List );

    //提取文件中各属性数据
    BOOL    bAttributeListFound = FALSE;        //用于判断是否含有ATTRIBUTE_LIST(0X20)属性
    PATTR_RECORD pAttr20 = NULL;                //指向0x20属性的数据
    PATTR_RECORD pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pRecordHeader + 
        pRecordHeader->attrs_offset);
    while( pAttrRecord->type != AT_END )
    {
        PFILE_ATTRIBUTE_NODE node = (PFILE_ATTRIBUTE_NODE)malloc( sizeof( FILE_ATTRIBUTE_NODE) );
        assert( node != NULL);
        RtlZeroMemory( node ,sizeof( FILE_ATTRIBUTE_NODE) );

        node->Length = pAttrRecord->length;
        node->AttributeType = pAttrRecord->type;
        node->AttributeData = malloc( pAttrRecord->length );
        node->AttrOffset = (WORD)((DWORD_PTR)pAttrRecord - (DWORD_PTR)pRecordHeader);
        node->OwnerRecordId = RecordId;
        assert( node->AttributeData !=  NULL );
        memcpy_s( node->AttributeData,pAttrRecord->length,pAttrRecord,
            node->Length );

        //检测0x30属性(常驻）,获得文件名信息
        if( pAttrRecord->type == AT_FILE_NAME && pAttrRecord->name_length == 0 )
        {
            assert( pAttrRecord->non_resident == 0 );
            PFILE_NAME_ATTR pFileNameAttr = (PFILE_NAME_ATTR)((DWORD_PTR)node->AttributeData + pAttrRecord->value_offset);
            
            if( pFileNameAttr->file_name_type == FILE_NAME_WIN32_AND_DOS ||
                pFileNameAttr->file_name_type == FILE_NAME_WIN32 )
            {
                pFileInfor->FileName = pFileNameAttr->file_name;
                pFileInfor->FileNameLength = pFileNameAttr->file_name_length;
            }
        }

        //检测0x80属性,获得文件大小(不包含命名流）
        if( pAttrRecord->type == AT_DATA && pAttrRecord->name_length == 0 )
        {
            if( pAttrRecord->non_resident == 0 )
            {
                //常驻
                pFileInfor->FileSize += pAttrRecord->value_length;
            }
            else
            {
                //非常驻
                pFileInfor->FileSize += pAttrRecord->data_size;
            }
        }


        //检测是否为ATTRIBUTE_LIST属性,并设置相应标记进行后继处理
        if( !bAttributeListFound && pAttrRecord->type == AT_ATTRIBUTE_LIST )
        {
            bAttributeListFound = TRUE;
            pAttr20 = (PATTR_RECORD)node->AttributeData;
        }

        InsertTailList( &pFileInfor->List,&node->List );
        node = NULL;

        pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
    }

    //if( bAttributeListFound )
    //{
     //   printf("%ld\n",pRecordHeader->mft_record_number );
      //  goto exit;
   // }

    if( bAttributeListFound )
    {
        //处理含有ATTRIBUTE_LIST属性的情况
        //_asm int 3;
        DbgPrint("Warning:ATTRIBUTE_LIST found!");

        //提取ATTRIBUTE_LIST属性值数据(注意:ATTRIBUTE_LIST属性即使为非常驻其dataruns也不会超出文件记录的范围）
        DWORD AttrListSize = GetAttributeListValue( pAttr20,NULL,0,NULL);
        assert( AttrListSize > 0 );
        LPBYTE  pValueBuffer = (LPBYTE)malloc(AttrListSize);
        assert( pValueBuffer != NULL);
        if( 0 > GetAttributeListValue( pAttr20,pValueBuffer,AttrListSize,&AttrListSize))
        {
            DbgPrint("GetAttrbuteList failed!");
            free( pValueBuffer );
            pValueBuffer = NULL;
            goto exit;
        }

        //遍历各个ATTR_LIST_ENTRY,并按照遍历次序将非基本文件记录中的属性添加到pFileInfor中
        //LONGLONG last_rec_id = -1;
        map<LONGLONG,BYTE> mymap;
        for( PATTR_LIST_ENTRY pAttrListEntry = (PATTR_LIST_ENTRY)pValueBuffer;
            pAttrListEntry < (PATTR_LIST_ENTRY)((DWORD_PTR)pValueBuffer + AttrListSize );
            pAttrListEntry = (PATTR_LIST_ENTRY)((DWORD_PTR)pAttrListEntry + pAttrListEntry->length))
        {
            //遍历完成，退出
            if( pAttrListEntry->type == AT_UNUSED ||
                pAttrListEntry->length == 0 ||
                pAttrListEntry->mft_reference == 0)
                break;

            //已经处理过，不再处理
            //if( last_rec_id == (pAttrListEntry->mft_reference & 0x0000ffffffffffffull) )
            //    continue;
            //last_rec_id = pAttrListEntry->mft_reference & 0x0000ffffffffffffull;

            if( mymap.find(pAttrListEntry->mft_reference & 0x0000ffffffffffffull) != mymap.end())
                continue;
            mymap[pAttrListEntry->mft_reference & 0x0000ffffffffffffull]=1;

            BOOL    bOk = FALSE;
            PMFT_RECORD pRecordHeader2 = (PMFT_RECORD)malloc( m_MftRecordLength );
            assert( pRecordHeader2 != NULL );
            RtlZeroMemory( pRecordHeader2,m_MftRecordLength );
            bOk = ReadMftRecord( pAttrListEntry->mft_reference & 0x0000ffffffffffffull,
                pRecordHeader2,
                m_MftRecordLength );
            assert( bOk );                  /*注意:ReadMftRecord需要m_MftDataRuns事先被初始化,然而初始化该
                                            变量的函数也要调用本函数InitNtfsFile.如果此时ReadMftRecord返回失败
                                            则可能是m_MftDataRuns还未被初始化。这种情况对应$MFT的文件记录中包含
                                            ATTRIBUTE_LIST 0X20属性,这里断言$MFT不会含有该属性*/

            //如果属性在基本文件记录中,则忽略,因为上边已经提取过这类属性
            if( pRecordHeader2->base_mft_record == 0 )
            {
                //last_rec_id = pAttrListEntry->mft_reference & 0x0000ffffffffffffull;
                mymap[pAttrListEntry->mft_reference & 0x0000ffffffffffffull] = 1;
                free( pRecordHeader2 );
                pRecordHeader2 = NULL;
                continue;
            }


            PWORD   pUsa2 = (PWORD)((DWORD_PTR)pRecordHeader2 + pRecordHeader2->usa_ofs);
            WORD    cUsa2 = pRecordHeader2->usa_count;
            
            //用USA数组更新扇区末尾两个字节的数据
            for( WORD i = 1;i < cUsa2;i++)
            {
                assert( *(PWORD)((DWORD_PTR)pRecordHeader2 + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) == pUsa2[0]);
                *(PWORD)((DWORD_PTR)pRecordHeader2 + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa2[i];
            }

            pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pRecordHeader2 + 
                pRecordHeader2->attrs_offset);
            while( pAttrRecord->type != AT_END )
            {
                PFILE_ATTRIBUTE_NODE node = (PFILE_ATTRIBUTE_NODE)malloc( sizeof( FILE_ATTRIBUTE_NODE) );
                assert( node != NULL);
                RtlZeroMemory( node ,sizeof( FILE_ATTRIBUTE_NODE) );

                node->Length = pAttrRecord->length;
                node->AttributeType = pAttrRecord->type;
                node->AttrOffset = (WORD)((DWORD_PTR)pAttrRecord - (DWORD_PTR)pRecordHeader2);
                node->OwnerRecordId = pAttrListEntry->mft_reference & 0x0000ffffffffffffull;
                node->AttributeData = malloc( pAttrRecord->length );
                assert( node->AttributeData !=  NULL );
                memcpy_s( node->AttributeData,pAttrRecord->length,pAttrRecord,
                    node->Length );

                //检测0x30属性(常驻）,获得文件名
                if( pAttrRecord->type == AT_FILE_NAME && pAttrRecord->name_length == 0 )
                {
                    assert( pAttrRecord->non_resident == 0 );
                    PFILE_NAME_ATTR pFileNameAttr = (PFILE_NAME_ATTR)((DWORD_PTR)node->AttributeData + pAttrRecord->value_offset);
                    
                    if( pFileNameAttr->file_name_type == FILE_NAME_WIN32_AND_DOS ||
                        pFileNameAttr->file_name_type == FILE_NAME_WIN32 )
                    {
                        pFileInfor->FileName = pFileNameAttr->file_name;
                        pFileInfor->FileNameLength = pFileNameAttr->file_name_length;
                    }
                }

                //检测0x80属性,获得文件大小(不包含命名流）
                if( pAttrRecord->type == AT_DATA && pAttrRecord->name_length == 0 )
                {
                    if( pAttrRecord->non_resident == 0 )
                    {
                        //常驻
                        pFileInfor->FileSize += pAttrRecord->value_length;
                    }
                    else
                    {
                        //非常驻
                        pFileInfor->FileSize += pAttrRecord->data_size;
                    }
                }

                InsertTailList( &pFileInfor->List,&node->List );
                node = NULL;

                pAttrRecord = (PATTR_RECORD)((DWORD_PTR)pAttrRecord + pAttrRecord->length);
            }//end while

            free( pRecordHeader2 );
            pRecordHeader2 = NULL;

        }//end for 遍历ATTR_LIST_ENTRY

        free( pValueBuffer );
        pValueBuffer = NULL;

        //注意:在将来提取非常驻属性值的时候,注意跨文件记录的dataruns的处理
    }//end if bAttributeListFound

exit:
    //恢复参数缓冲区到初始状态
    for( WORD i = 1;i < cUsa;i++)
    {
        *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[0];
    }

    return pFileInfor;
}

PFILE_INFORMATION CNtfsController::OpenNtfsFile( IN LONGLONG RecordId )
/*++
功能描述:打开文件,返回文件属性信息结构指针

参数:RecordId:文件记录号

返回值:成功返回文件属性信息结构指针,失败返回NULL
--*/
{
    BOOL    bResult = TRUE;
    LPVOID  buffer = malloc( m_MftRecordLength );
    assert( buffer != NULL );
    bResult = ReadMftRecord( RecordId,buffer,m_MftRecordLength );
    if( !bResult )
    {
        free( buffer );
        return NULL;
    }
    PFILE_INFORMATION file = InitNtfsFile( buffer,m_MftRecordLength,RecordId);
    if( file != NULL )
        file->FileRecordId = RecordId;

    free( buffer );
    return file;
}

BOOL CNtfsController::ReadMftRecord( IN LONGLONG RecordId,OUT PVOID Buffer,IN DWORD BufferLength )
/*++
功能描述:读取$MFT文件中指定文件记录号的文件记录

参数:
    RecordId:文件记录号
    Buffer:输出缓冲区,调用者负责内存的分配和释放
    BufferLength:输出缓冲区长度（单位:字节）,至少为一个文件记录大小

返回值:成功返回TRUE,失败返回FALSE

--*/
{
    assert( Buffer != NULL );
    assert( BufferLength >= m_MftRecordLength );

    if( m_MftDataRuns == NULL ){
        DbgPrint("dataruns == null!\n");
        return FALSE;                       //$MFT尚未被初始化返回失败
    }

    BOOL bResult = TRUE;

    //计算RecordId对应文件记录所在的VCN
    LONGLONG    vcn = m_MftRecordLength / m_BootSect.bpb.bytes_per_sector * RecordId
        / m_BootSect.bpb.sectors_per_cluster;

    //转换为LCN,出错返回失败
    LONGLONG    lcn = VcnToLcn( vcn,m_MftDataRuns,m_MftDataRunsLength );
    if( lcn == -1 )
    {
        DbgPrint("vcn to lcn failed!\n");

        return FALSE;
    }

    LONGLONG   offsetInSectors = RecordId                                           //文件记录号
        * (m_MftRecordLength / m_BootSect.bpb.bytes_per_sector) //每文件记录扇区数
        % m_BootSect.bpb.sectors_per_cluster;                   //每簇扇区数
    bResult = TRUE;
    for( DWORD i = 0;i < (m_MftRecordLength / m_BootSect.bpb.bytes_per_sector);i++)
    {
        bResult = ReadLogicalSector( (LPVOID)((DWORD_PTR)Buffer + i * m_BootSect.bpb.bytes_per_sector),
            m_BootSect.bpb.bytes_per_sector,
            lcn * m_BootSect.bpb.sectors_per_cluster + offsetInSectors + i );
        if( !bResult ){
            printf("read failed!\n");
            goto exit;
        }
    }

    bResult = ntfs_is_file_recordp(Buffer);
    if( bResult==FALSE){
        DbgPrint("Not file record,return false!");
    }

exit:
    return bResult;

}

BOOL CNtfsController::WriteMftRecord( IN LONGLONG RecordId,IN PVOID Buffer,IN DWORD BufferLength )
/*++
功能描述:写$MFT文件中指定文件记录号的文件记录

参数:
    RecordId:文件件记录号
    Buffer:输入缓冲区,调用者负责内存的分配和释放
    BufferLength:输入缓冲区长度（单位:字节）,至少为一个文件记录大小

返回值:成功返回TRUE,失败返回FALSE

--*/
{
    BOOL bResult = TRUE;

    assert( Buffer != NULL );
    assert( BufferLength >= m_MftRecordLength );

    bResult = ntfs_is_file_recordp(Buffer);
    if( bResult==FALSE){
        DbgPrint("Not file record,return false!");
        goto exit;
    }

    if( m_MftDataRuns == NULL ){
        DbgPrint("dataruns == null!\n");
        bResult = FALSE;
        goto exit;                  //$MFT尚未被初始化返回失败
    }

    //计算RecordId对应文件记录所在的VCN
    LONGLONG    vcn = m_MftRecordLength / m_BootSect.bpb.bytes_per_sector * RecordId
        / m_BootSect.bpb.sectors_per_cluster;

    //转换为LCN,出错返回失败
    LONGLONG    lcn = VcnToLcn( vcn,m_MftDataRuns,m_MftDataRunsLength );
    if( lcn == -1 )
    {
        DbgPrint("vcn to lcn failed!\n");
        bResult = FALSE;
        goto exit;
    }

    //计算文件记录扇区相对所在簇的偏移量（单位为扇区）
    LONGLONG   offsetInSectors = RecordId                                           //文件记录号
        * (m_MftRecordLength / m_BootSect.bpb.bytes_per_sector) //每文件记录扇区数
        % m_BootSect.bpb.sectors_per_cluster;                   //每簇扇区数

    bResult = TRUE;
    for( DWORD i = 0;i < (m_MftRecordLength / m_BootSect.bpb.bytes_per_sector);i++)
    {
        bResult = WriteLogicalSector( (LPVOID)((DWORD_PTR)Buffer + i * m_BootSect.bpb.bytes_per_sector),
            m_BootSect.bpb.bytes_per_sector,
            lcn * m_BootSect.bpb.sectors_per_cluster + offsetInSectors + i );
        if( !bResult ){
            printf("write failed!\n");
            goto exit;
        }
    }

exit:
    return bResult;
}

VOID CNtfsController::CloseFile(PFILE_INFORMATION File )
/*++
功能描述:关闭文件,释放资源

参数:File:文件属性信息结构指针

返回值:无

说明:不能重复关闭文件,否则将会崩溃

--*/
{
    PLIST_ENTRY     list = NULL;

    if( File == NULL )return;

    while( !IsListEmpty( &File->List ))
    {
        list = RemoveHeadList( &File->List );
        assert( list != NULL );
        PFILE_ATTRIBUTE_NODE p = (PFILE_ATTRIBUTE_NODE)CONTAINING_RECORD( list,
            FILE_ATTRIBUTE_NODE,
            List );
        if( p->AttributeData != NULL )
            free( p->AttributeData );
        free( p );
    }
    free( File );
}

LONG CNtfsController::GetAttributeListValue( IN PATTR_RECORD AttrRecord,
                                            OUT PVOID Buffer,
                                            IN DWORD BufferLength,
                                            OUT PDWORD BytesReturned )
/*++
功能描述:获取ATTRIBUTE_LIST属性中值数据

参数说明:
    AttrRecord:指向ATTRIBUTE_LIST属性数据的指针
    Buffer:输出缓冲区
    BufferLength:输出缓冲区长度（字节）
    BytesReturned:指向用于接收输出缓冲区有效数据字节数的DWORD变量

返回值:成功返回0,失败返回 -1,如果Buffer为空,返回所需最少的Buffer长度（大于0)

--*/
{

    if( AttrRecord == NULL || AttrRecord->type != AT_ATTRIBUTE_LIST )
    {
        return -1;
    }

    if( AttrRecord->non_resident == 0 )
    {
        //ATTRIBUTE_LIST为常驻
        if( Buffer == NULL )
        {
            return AttrRecord->value_length;
        }

        if( BufferLength < AttrRecord->value_length )
        {
            return -1;
        }

        memcpy_s( Buffer,
            BufferLength,
            (LPVOID)((DWORD_PTR)AttrRecord + AttrRecord->value_offset),
            AttrRecord->value_length );
        if( BytesReturned != NULL)
            *BytesReturned = AttrRecord->value_length;

        return 0;
    }//常驻结束
    else
    {
        //ATTRIBUTE_LIST为非常驻
        LPBYTE  DataRuns = (LPBYTE)((DWORD_PTR)AttrRecord + AttrRecord->mapping_pairs_offset);
        DWORD   DataRunsLength = AttrRecord->length - AttrRecord->mapping_pairs_offset;
        LONGLONG NumberOfClusters = GetNumberOfVcnsInDataRuns(DataRuns,DataRunsLength);
        DWORD bytesSize = (DWORD)NumberOfClusters * m_BootSect.bpb.sectors_per_cluster *
            m_BootSect.bpb.bytes_per_sector;

        if(Buffer == NULL )
        {
            return bytesSize;
        }

        if( BufferLength < bytesSize )
        {
            return -1;
        }

        for( LONGLONG vcn = 0;vcn < NumberOfClusters;vcn++)
        {
            LONGLONG lcn = VcnToLcn( vcn,DataRuns,DataRunsLength );
            if( lcn == -1 )break;

            if( !ReadLogicalCluster( (LPBYTE)Buffer + vcn * m_ClusterSizeInBytes,
                m_ClusterSizeInBytes,
                lcn ))
                return -1;
        }
        if( BytesReturned != NULL)
            *BytesReturned = (DWORD)AttrRecord->data_size;

        return 0;
    }//非常驻结束

    return 0;
}

LONG CNtfsController::GetAttributeValue( IN NTFS_FILE File,
                                        IN ATTR_TYPES Type,
                                        OUT PVOID Buffer,
                                        IN DWORD BufferLength,
                                        OUT PBOOL IsDataruns,
                                        IN PWCHAR AttrName,
                                        IN WORD Instance,
                                        OUT PDWORD BytesReturned)
/*++
功能描述:提取属性值数据,常驻直接返回数据,非常驻返回dataruns数据,用于进一步读取

参数:
    File:NTFS_FILE 描述文件信息的结构
    Type:属性类型
    Buffer:输出缓冲区
    BufferLength:指出输出缓冲区长度
    IsDataruns:指向BOOL变量,用于接收输出缓冲区的内容是否为Dataruns
    BytesReturned:实际返回的字节数
    AttrName:指向属性名的UNICODE字符串(以L'\0'结尾）,默认为NULL（无属性名）
    Instance:属性实例ID,默认为0,表示忽略,对于0x10属性忽略此参数

返回值:成功返回0,失败返回 -1,如果Buffer为空,返回所需最少的Buffer长度（大于0)

说明:要对跨文件记录的属性进行处理
--*/
{
    LONG result = -1;

    for( PLIST_ENTRY list = File->List.Flink;
        list != &File->List;
        list = list->Flink)
    {
        PFILE_ATTRIBUTE_NODE node = 
            (PFILE_ATTRIBUTE_NODE)CONTAINING_RECORD( list,FILE_ATTRIBUTE_NODE,List);
        PATTR_RECORD AttrRecord = (PATTR_RECORD)node->AttributeData;

        if( node->AttributeType != Type)continue;

        if( AttrName != NULL )
        {
            if( AttrRecord->name_length == 0 )continue;
            if( wcsncmp( (PWCHAR)((DWORD_PTR)AttrRecord + AttrRecord->name_offset),
                AttrName,
                AttrRecord->name_length ) != 0 )
                continue;
        }

        if( node->AttributeType != AT_STANDARD_INFORMATION
            && Instance !=0 && Instance != AttrRecord->instance)
            continue;

        if( AttrRecord->non_resident == 0 )
        {
            //常驻属性的处理,一般常驻属性不会出现跨文件记录的情况
            if( Buffer == NULL )
            {
                result = AttrRecord->value_length;
                break;
            }

            if( BufferLength < AttrRecord->value_length )
            {
                result = -1;
                break;
            }

            memcpy_s(   Buffer,
                BufferLength,
                (LPVOID)((DWORD_PTR)AttrRecord + AttrRecord->value_offset),
                AttrRecord->value_length );
            if( BytesReturned != NULL)*BytesReturned = AttrRecord->value_length;
            if( IsDataruns != NULL )*IsDataruns = FALSE;

            result = 0;
            break;

        }
        else
        {
            //非常驻属性的处理,注意跨文件记录的情况
            DWORD valueLength = AttrRecord->length - AttrRecord->mapping_pairs_offset;
            LPVOID tmpBuffer = malloc( valueLength );
            assert( tmpBuffer != NULL);
            memcpy_s( tmpBuffer,
                valueLength,
                (LPVOID)((DWORD_PTR)AttrRecord + AttrRecord->mapping_pairs_offset),
                valueLength );

            PLIST_ENTRY list2 = NULL;
            for(list2 = list->Flink;
                list2 != &File->List;
                list2 = list2->Flink )
            {
                PFILE_ATTRIBUTE_NODE node2 = 
                    (PFILE_ATTRIBUTE_NODE)CONTAINING_RECORD( list2,FILE_ATTRIBUTE_NODE,List);
                PATTR_RECORD AttrRecord2 = (PATTR_RECORD)node2->AttributeData;

                if( node2->AttributeType != Type )break;
                if( AttrRecord->name_length != AttrRecord2->name_length)break;
                if( AttrRecord->name_length > 0 )
                {
                    if( wcsncmp( (PWCHAR)((DWORD_PTR)AttrRecord2 + AttrRecord2->name_offset),
                        (PWCHAR)((DWORD_PTR)AttrRecord + AttrRecord->name_offset),
                        AttrRecord2->name_length ) != 0 )
                        break;
                }
                if( AttrRecord2->non_resident == 0 )break;
                assert( AttrRecord2->lowest_vcn > 0 && AttrRecord2->instance == 0 );

                //拼接dataruns
                LONGLONG lastLcn = GetLastStartLcnInDataruns( (LPBYTE)tmpBuffer,valueLength );
                LPBYTE dataruns2 = (LPBYTE)((DWORD_PTR)AttrRecord2 + AttrRecord2->mapping_pairs_offset);
                DWORD_PTR offset_p = GetDataRunsLength( (LPBYTE)tmpBuffer,valueLength );
                LPBYTE p = (LPBYTE)((DWORD_PTR)tmpBuffer + offset_p);
                WORD cBytes = 0;
                if( (*dataruns2 & 0xf0) == 0 )
                {
                    valueLength += *dataruns2;
                    tmpBuffer = realloc( tmpBuffer,valueLength );
                    assert( tmpBuffer != NULL);
                    p = (LPBYTE)((DWORD_PTR)tmpBuffer + offset_p);
                    memcpy_s( p,
                        *dataruns2 + 1,
                        dataruns2,
                        *dataruns2 + 1);
                    cBytes = *dataruns2 + 1;
                    p+= cBytes;
                    dataruns2 += cBytes;
                }
                *p = 0;
                if( *dataruns2 != 0 ){
                    LONGLONG newStartLcn = VcnToLcn( 0,dataruns2,AttrRecord2->length - AttrRecord2->mapping_pairs_offset - cBytes)
                                            -lastLcn;
                    BYTE len_newStartLcn = CompressLongLong( newStartLcn );
                    cBytes = *dataruns2 & 0x0f;
                    valueLength += cBytes +len_newStartLcn+1;
                    offset_p = (DWORD_PTR)p - (DWORD_PTR)tmpBuffer;
                    tmpBuffer = realloc( tmpBuffer,valueLength );
                    assert( tmpBuffer != NULL);
                    p = (LPBYTE)((DWORD_PTR)tmpBuffer + offset_p);
                    *p++ = (len_newStartLcn << 4) | (BYTE)cBytes;
                    memcpy_s( p,
                            cBytes,
                            dataruns2+1,
                            cBytes);
                    p += cBytes;
                    memcpy_s( p,
                            len_newStartLcn,
                            &newStartLcn,
                            len_newStartLcn );
                    p += len_newStartLcn;
                    *p = 0;
                    cBytes = (*dataruns2 & 0x0f)+((*dataruns2 & 0xf0)>>4)+1;
                    dataruns2 += cBytes;
                    offset_p = (DWORD_PTR)p - (DWORD_PTR)tmpBuffer;
                    valueLength += AttrRecord2->length - AttrRecord2->mapping_pairs_offset;//多加了点，懒得精确计算了
                    tmpBuffer = realloc( tmpBuffer,valueLength );
                    assert( tmpBuffer != NULL);
                    p = (LPBYTE)((DWORD_PTR)tmpBuffer + offset_p);
                    DWORD remain_len = (DWORD)(((DWORD_PTR)AttrRecord2 + AttrRecord2->length)
                                            - (DWORD_PTR)dataruns2);
                    memcpy_s( p,
                             remain_len,
                             dataruns2,
                             remain_len );
                    p+= remain_len;
                    *p = 0;
                    while( valueLength % 8 != 0 )
                        valueLength++;
                    tmpBuffer = realloc( tmpBuffer,valueLength);
                    assert( tmpBuffer != NULL);
                }
                    
            }// end for list2

            if( IsDataruns != NULL )*IsDataruns = TRUE;

            if( Buffer == NULL)
            {
                result = valueLength;
                free( tmpBuffer );
                tmpBuffer = NULL;
                break;
            }

            if( BufferLength < valueLength )
            {
                result = -1;
                free( tmpBuffer );
                tmpBuffer = NULL;
                break;
            }

            memcpy_s( Buffer,
                BufferLength,
                tmpBuffer,
                valueLength );
            if( BytesReturned != NULL)*BytesReturned = valueLength;

            free( tmpBuffer );
            tmpBuffer = NULL;
            result = 0;
            break;
        }//end else non_resident

    }//end for list

    return result;
}

VOID CNtfsController::UpdateBitmapFromBlockList( PLIST_ENTRY ListHead )
/*++
功能描述：从块信息链表中更新Bitmap

参数：块信息链表头

返回值：无

说明：若块描述类型为BLOCK_TYPE_FREE，则相应位置0，若为BLOCK_TYPE_USED/BAD/DEAD则置1
--*/
{
    assert( ListHead != NULL);

    for( PLIST_ENTRY list = ListHead->Flink;
        list != ListHead;
        list = list->Flink)
    {
        PBLOCK_DESCRIPTOR block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                        BLOCK_DESCRIPTOR,
                                                                        List );
        LONGLONG startCluster = block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster;
        LONGLONG totalClusters = block->TotalSectors.QuadPart % m_BootSect.bpb.sectors_per_cluster == 0 ?
                                 block->TotalSectors.QuadPart / m_BootSect.bpb.sectors_per_cluster:
                                 block->TotalSectors.QuadPart / m_BootSect.bpb.sectors_per_cluster + 1;

        if( block->type == BLOCK_TYPE_FREE )
        {
            for( LONGLONG i = startCluster;i < startCluster+totalClusters;i++)
                m_Bitmap[ i / 8 ] &= ~(1 << (i % 8));
        }
        else
        {
            for( LONGLONG i = startCluster;i < startCluster+totalClusters;i++)
                m_Bitmap[ i / 8 ] |= (1 << (i % 8));
        }

    }// end for list
}

LONGLONG CNtfsController::AllocateBlock(LONGLONG LengthInCluster)
/*++
功能描述：分配一个指定长度的空闲块（依据Bitmap）

参数：所根本块的大小，单位为簇

返回值：成功返回所分配块的首簇号，失败返回负值，其绝对值为目前可分配的最大块长

说明：需要调整Bitmap和FreeBlockList
--*/
{
    LONGLONG maxBlock = -1,result = -1;
    PLIST_ENTRY list = NULL;
    char message[1024]={0};
    sprintf_s( message,1024,"allocate %x clusters",LengthInCluster );
    DbgPrint( message);
    DbgPrint("show list ...");
    _ShowList();

    for( list = m_BlockInforHead.FreeBlockList.Flink;
        list != &m_BlockInforHead.FreeBlockList;
        )
    {
        PBLOCK_DESCRIPTOR block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                        BLOCK_DESCRIPTOR,
                                                                        List);
        list = list->Flink;

        LONGLONG start = block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster;
        LONGLONG len = block->TotalSectors.QuadPart / m_BootSect.bpb.sectors_per_cluster;


        if( LengthInCluster < len )
        {
            block->TotalSectors.QuadPart -= LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
            m_BlockInforHead.FreeBlockSize.QuadPart -= LengthInCluster * m_ClusterSizeInBytes;
            start += block->TotalSectors.QuadPart / m_BootSect.bpb.sectors_per_cluster;

            result =  start;
            break;
        }
        else if( LengthInCluster == len )
        {
            m_BlockInforHead.FreeBlockSize.QuadPart -= LengthInCluster * m_ClusterSizeInBytes;
            RemoveEntryList( &block->List );
            free( block );
            block = NULL;

            result =  start;
            break;
        }

        else
        {
            if( maxBlock < len )
                maxBlock = len;
        }
    }

    if( result != -1 )
    {
        //调整Bitmap
        for(LONGLONG i = result;i < result + LengthInCluster;i++)
            m_Bitmap[ i / 8 ] |= (1 << (i % 8));

        //调整已用块链表
        m_BlockInforHead.UsedBlockSize.QuadPart += LengthInCluster * m_ClusterSizeInBytes;
        
        if( IsListEmpty( &m_BlockInforHead.UsedBlockList))
        {
            PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
            assert( node != NULL );

            RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
            node->StartSector.QuadPart = result * m_BootSect.bpb.sectors_per_cluster;
            node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
            node->type = BLOCK_TYPE_USED;
            InsertHeadList( &m_BlockInforHead.UsedBlockList,&node->List );
            node = NULL;

            return result;
        }

        PBLOCK_DESCRIPTOR first_block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(
            m_BlockInforHead.UsedBlockList.Flink,
            BLOCK_DESCRIPTOR,
            List);
        PBLOCK_DESCRIPTOR last_block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(
            m_BlockInforHead.UsedBlockList.Blink,
            BLOCK_DESCRIPTOR,
            List);

        if( result <= first_block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster )
        {
            if( result + LengthInCluster >= 
                first_block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster )
            {
                first_block->TotalSectors.QuadPart = 
                    first_block->StartSector.QuadPart + first_block->TotalSectors.QuadPart
                    - result * m_BootSect.bpb.sectors_per_cluster;
                first_block->StartSector.QuadPart = result * m_BootSect.bpb.sectors_per_cluster;
            }
            else
            {
                PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
                assert( node != NULL );

                RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
                node->StartSector.QuadPart = result * m_BootSect.bpb.sectors_per_cluster;
                node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
                node->type = BLOCK_TYPE_USED;
                InsertHeadList( &m_BlockInforHead.UsedBlockList,&node->List );
                node = NULL;
            }
        }
        else if(result >= last_block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster)
        {
            if( (last_block->StartSector.QuadPart + last_block->TotalSectors.QuadPart)
                / m_BootSect.bpb.sectors_per_cluster 
                >= result )
            {
                last_block->TotalSectors.QuadPart = (result + LengthInCluster)
                    * m_BootSect.bpb.sectors_per_cluster - last_block->StartSector.QuadPart;
            }
            else
            {
                PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
                assert( node != NULL );

                RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
                node->StartSector.QuadPart = result * m_BootSect.bpb.sectors_per_cluster;
                node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
                node->type = BLOCK_TYPE_USED;
                InsertTailList( &m_BlockInforHead.UsedBlockList,&node->List );
                node = NULL;
            }
        }
        else{

            for( list = m_BlockInforHead.UsedBlockList.Flink;
                list != &m_BlockInforHead.UsedBlockList;
                )
            {
                //上边已经排除这种情况
                assert( list != m_BlockInforHead.UsedBlockList.Blink );

                PBLOCK_DESCRIPTOR block_prev = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                            BLOCK_DESCRIPTOR,
                                                                            List);
                list = list->Flink;
                PBLOCK_DESCRIPTOR block_next = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                            BLOCK_DESCRIPTOR,
                                                                            List);
                //寻找插入点
                if( result > block_next->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster)
                    continue;
                
                //处理归并块的四种情况

                BOOLEAN bAdjPrev = (
                    result > block_prev->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster &&
                    result <= (block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart)
                    / m_BootSect.bpb.sectors_per_cluster);
                BOOLEAN bAdjNext = (
                    result + LengthInCluster >= block_next->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster &&
                    result + LengthInCluster < (block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart)
                    / m_BootSect.bpb.sectors_per_cluster);

                if( bAdjPrev && bAdjNext )
                {
                    //前后均相邻
                    block_prev->TotalSectors.QuadPart = 
                        block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart
                        - block_prev->StartSector.QuadPart;
                    RemoveEntryList( &block_next->List );
                    free( block_next );
                    block_next = NULL;
                }
                else if ( bAdjPrev )
                {
                    block_prev->TotalSectors.QuadPart = 
                        (result + LengthInCluster) * m_BootSect.bpb.sectors_per_cluster
                        - block_prev->StartSector.QuadPart;
                }
                else if( bAdjNext )
                {
                    block_next->TotalSectors.QuadPart = 
                        block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart
                        - result * m_BootSect.bpb.sectors_per_cluster;
                    block_next->StartSector.QuadPart = result * m_BootSect.bpb.sectors_per_cluster;
                }
                else
                {
                    PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
                    assert( node != NULL );

                    RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
                    node->StartSector.QuadPart = result * m_BootSect.bpb.sectors_per_cluster;
                    node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
                    node->type = BLOCK_TYPE_USED;
                    InsertHeadList( &block_prev->List,&node->List );
                    node = NULL;
                }// end if bAdj....
                
                break;
            }// end for list

        }//end if
        sprintf_s( message,1024,"allocate: %lld %lld\n",result,LengthInCluster );
        DbgPrint( message);
        DbgPrint("show list 2");
        _ShowList();
        return result;
    }//end if result != -1

    DbgPrint(" allocate failed!");
    return -maxBlock;
}

BOOL CNtfsController::FreeBlock( LONGLONG StartCluster,LONGLONG LengthInCluster )
/*++
功能描述：释放可用空间

参数：
    StartCluster:起始簇号
    LengthInCluster:块长度，单位为簇

返回值：成功返回TRUE，失败返回FALSE

--*/
{
    CHAR buffer[1024] = {0};
    BOOL bResult = FALSE;

    sprintf_s(buffer,sizeof(buffer),"FreeBlock(%lld,%lld)",StartCluster,LengthInCluster);
    DbgPrint( buffer );
    DbgPrint("show list...");
    _ShowList();
    PLIST_ENTRY list = NULL;
    for( list = m_BlockInforHead.UsedBlockList.Flink;
        list != &m_BlockInforHead.UsedBlockList;
        )
    {
        PBLOCK_DESCRIPTOR block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                        BLOCK_DESCRIPTOR,
                                                                        List);
        list = list->Flink;

        LONGLONG start = block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster;
        LONGLONG len = block->TotalSectors.QuadPart / m_BootSect.bpb.sectors_per_cluster;
        
        //释放块必须属于某占用块
        if(!( StartCluster >= start && StartCluster + LengthInCluster <= start + len))
            continue;
        
        m_BlockInforHead.UsedBlockSize.QuadPart -= LengthInCluster * m_ClusterSizeInBytes;
        
        if( StartCluster == start )
        {
            block->StartSector.QuadPart += LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
            block->TotalSectors.QuadPart -= LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
            if( block->TotalSectors.QuadPart == 0 )
            {
                RemoveEntryList(&block->List);
                free( block );
            }

        }
        else
        {

            if( start + len > StartCluster + LengthInCluster)
            {
                //增加节点

                PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
                assert( node != NULL );
                RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));

                node->StartSector.QuadPart = 
                    (StartCluster + LengthInCluster) * m_BootSect.bpb.sectors_per_cluster;
                node->TotalSectors.QuadPart = 
                    (start + len - StartCluster - LengthInCluster ) * m_BootSect.bpb.sectors_per_cluster;
                node->type = BLOCK_TYPE_USED;
                InsertHeadList( &block->List,&node->List );
                node = NULL;
            }
            
            block->TotalSectors.QuadPart = 
                (StartCluster - start ) * m_BootSect.bpb.sectors_per_cluster;

        }

        bResult = TRUE;
        break;

    }

    if( !bResult )goto exit;

    //调整Bitmap
    for(LONGLONG i = StartCluster;i < StartCluster + LengthInCluster;i++)
        m_Bitmap[ i / 8 ] &= ~(1 << (i % 8));

    //调整空闲块链表
    m_BlockInforHead.FreeBlockSize.QuadPart += LengthInCluster * m_ClusterSizeInBytes;
    
    if( IsListEmpty( &m_BlockInforHead.FreeBlockList))
    {
        PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
        assert( node != NULL );

        RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
        node->StartSector.QuadPart = StartCluster * m_BootSect.bpb.sectors_per_cluster;
        node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
        node->type = BLOCK_TYPE_USED;
        InsertHeadList( &m_BlockInforHead.FreeBlockList,&node->List );
        node = NULL;

        DbgPrint("notice!");
        return bResult;
    }

    PBLOCK_DESCRIPTOR first_block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(
        m_BlockInforHead.FreeBlockList.Flink,
        BLOCK_DESCRIPTOR,
        List);
    PBLOCK_DESCRIPTOR last_block = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD(
            m_BlockInforHead.FreeBlockList.Blink,
            BLOCK_DESCRIPTOR,
            List);

    if( StartCluster <= first_block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster )
    {
        if( StartCluster + LengthInCluster >= 
            first_block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster )
        {
            first_block->TotalSectors.QuadPart = 
                first_block->StartSector.QuadPart + first_block->TotalSectors.QuadPart
                - StartCluster * m_BootSect.bpb.sectors_per_cluster;
            first_block->StartSector.QuadPart = StartCluster * m_BootSect.bpb.sectors_per_cluster;

        }
        else
        {
            PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
            assert( node != NULL );

            RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
            node->StartSector.QuadPart = StartCluster * m_BootSect.bpb.sectors_per_cluster;
            node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
            node->type = BLOCK_TYPE_USED;
            InsertHeadList( &m_BlockInforHead.FreeBlockList,&node->List );
            node = NULL;
        }
    }
    else if(StartCluster >= last_block->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster)
    {
        if( (last_block->StartSector.QuadPart + last_block->TotalSectors.QuadPart)
            / m_BootSect.bpb.sectors_per_cluster 
            >= StartCluster )
        {
            last_block->TotalSectors.QuadPart += LengthInCluster
                * m_BootSect.bpb.sectors_per_cluster;
        }
        else
        {
            PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
            assert( node != NULL );

            RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
            node->StartSector.QuadPart = StartCluster * m_BootSect.bpb.sectors_per_cluster;
            node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
            node->type = BLOCK_TYPE_USED;
            InsertTailList( &m_BlockInforHead.FreeBlockList,&node->List );
            node = NULL;
        }
    }
    else{

        for( list = m_BlockInforHead.FreeBlockList.Flink;
            list != &m_BlockInforHead.FreeBlockList;
            )
        {
            //上边已经排除这种情况
            assert( list != m_BlockInforHead.FreeBlockList.Blink );

            PBLOCK_DESCRIPTOR block_prev = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                        BLOCK_DESCRIPTOR,
                                                                        List);
            list = list->Flink;
            PBLOCK_DESCRIPTOR block_next = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list,
                                                                            BLOCK_DESCRIPTOR,
                                                                            List);
            //寻找插入点
            if( StartCluster > block_next->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster)
                continue;
                
            //处理归并块的四种情况

            BOOLEAN bAdjPrev = (
                StartCluster > block_prev->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster &&
                StartCluster <= (block_prev->StartSector.QuadPart + block_prev->TotalSectors.QuadPart)
                / m_BootSect.bpb.sectors_per_cluster);
            BOOLEAN bAdjNext = (
                StartCluster + LengthInCluster >= block_next->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster &&
                StartCluster + LengthInCluster < (block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart)
                / m_BootSect.bpb.sectors_per_cluster);

            if( bAdjPrev && bAdjNext )
            {
                //前后均相邻
                block_prev->TotalSectors.QuadPart = 
                    block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart
                    - block_prev->StartSector.QuadPart;
                RemoveEntryList( &block_next->List );
                free( block_next );
                block_next = NULL;
            }
            else if ( bAdjPrev )
            {
                block_prev->TotalSectors.QuadPart = 
                    (StartCluster + LengthInCluster) * m_BootSect.bpb.sectors_per_cluster
                    - block_prev->StartSector.QuadPart;
            }
            else if( bAdjNext )
            {
                block_next->TotalSectors.QuadPart = 
                    block_next->StartSector.QuadPart + block_next->TotalSectors.QuadPart
                    - StartCluster * m_BootSect.bpb.sectors_per_cluster;
                block_next->StartSector.QuadPart = StartCluster * m_BootSect.bpb.sectors_per_cluster;
            }
            else
            {
                PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)malloc( sizeof( BLOCK_DESCRIPTOR));
                assert( node != NULL );

                RtlZeroMemory( node,sizeof( BLOCK_DESCRIPTOR ));
                node->StartSector.QuadPart = StartCluster * m_BootSect.bpb.sectors_per_cluster;
                node->TotalSectors.QuadPart = LengthInCluster * m_BootSect.bpb.sectors_per_cluster;
                node->type = BLOCK_TYPE_USED;
                InsertHeadList( &block_prev->List,&node->List );
                node = NULL;
            }// end if bAdj....
                
            break;
        }// end for list

    }//else end

exit:
    if( bResult)
        DbgPrint("return true");
    else 
        DbgPrint("return false");
    DbgPrint("show list2...");
    _ShowList();

    return bResult;
}

BOOL CNtfsController::FreeBlockInDataruns(LPBYTE DataRuns,DWORD Length)
/*++
功能描述：释放dataruns描述的块占用的空间

参数：
    Dataruns:指向存放dataruns数据的缓冲区指针
    Length:dataruns数据的长度

返回值：成功返回TRUE,失败返回FALSE
--*/
{
    LONGLONG startLcn = 0,len = 0;
    BOOL bResult = TRUE;

    if( DataRuns == NULL)return FALSE;

    for(DWORD i = 0;
        i < Length;
        )
    {
        DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

        if( DataRuns[i] == 0 )break;

        cStartLcnBytes = ( DataRuns[i] & 0xf0) >> 4;//压缩字节的高4位
        cLenLcnBytes = (DataRuns[i] & 0x0f);        //压缩字节的低4位

        if( cStartLcnBytes != 0)
        {
            //提取当前数据流长度
            len = 0;
            for( DWORD j = cLenLcnBytes;j > 0;j--)
            {
                len = (len << 8) | DataRuns[i + j ]; 
            }

            //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
            LONGLONG tmp = 0;
            if( DataRuns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                tmp = -1ll;
            for( DWORD j = cStartLcnBytes;j > 0;j-- )
            {
                tmp = ( tmp << 8 ) | DataRuns[i + cLenLcnBytes + j ];
            }
            startLcn = startLcn + tmp;
            assert( startLcn >= 0 );

            bResult = FreeBlock( startLcn,len );
            if( !bResult )
            {
                DbgPrint("free block in dataruns failed!");
                goto exit;
            }
        }

        i += cStartLcnBytes + cLenLcnBytes + 1;
    }
exit:
    return bResult;
}

LONGLONG CNtfsController::AllocateFileRecordId()
/*++
功能描述：从文件记录分配一个可用的文件记录号，并将相应的位标记为占用

参数：无

返回值：成功返回所分配的文件记录号，失败返回-1
--*/
{
    LONGLONG file_id = -1;

    for( LONGLONG i = 3;
        i < (m_MftNumberOfRecord % 8==0?m_MftNumberOfRecord/8:m_MftNumberOfRecord/8 + 1);
        i++)
    {
        BYTE byte = m_MftBitmap[i];
        if( byte == 0xff)
            continue;

        for( int j = 0;j < 8;j++)
        {
            if( byte & (1 << j))
                continue;
            file_id = i * 8 + j;
            break;
        }
        break;
    }
    if( file_id != -1 )
        m_MftBitmap[ file_id / 8 ] |= (1 << file_id % 8);
    
    return file_id;
}

VOID CNtfsController::FreeFileRecordId( LONGLONG FileRecordId )
/*++
功能描述：从文件记录分配位图中释放指定的文件记录号

参数：无

返回值：无
--*/
{
    BOOL bOk = TRUE;
    assert( m_MftBitmap[ FileRecordId/8] & (1 << (FileRecordId % 8)));
    m_MftBitmap[ FileRecordId / 8 ] &= ~(1 << FileRecordId % 8);
    PMFT_RECORD pRecordHeader = (PMFT_RECORD)malloc( this->m_MftRecordLength);
    assert( pRecordHeader != NULL);

    bOk = ReadMftRecord( FileRecordId,pRecordHeader,m_MftRecordLength );
    assert( bOk );
    pRecordHeader->flags = MFT_RECORD_UNUSED;
    bOk = WriteMftRecord( FileRecordId,pRecordHeader,m_MftRecordLength );
    assert( bOk );
    free( pRecordHeader );
    pRecordHeader = NULL;

}

LONG CNtfsController::CheckAndUpdateFile(LONGLONG FileId )
/*++
功能描述：检查文件的存储空间是否在坏扇区区域内，若在则申请磁盘空间并转移数据，更新dataruns

参数:FileId:文件记录ID

返回值：
    0：文件不在坏扇区区域中，不需要调整
    1：文件受到坏扇区影响，并调整成功
    2：文件受到坏扇区影响，但无法调整
    -1:无效文件
--*/
{
    LPBYTE dataruns = NULL;
    LONGLONG len_dataruns = 0;
    LONG result = 0;

    NTFS_FILE hFile = OpenNtfsFile( FileId );
    if( hFile == NULL)
    {
        result = -1;
        goto exit;
    }

    for( PLIST_ENTRY list = hFile->List.Flink;
        list != &hFile->List;
        list = list->Flink)
    {
        PFILE_ATTRIBUTE_NODE node = (PFILE_ATTRIBUTE_NODE)CONTAINING_RECORD(
                                                    list,
                                                    FILE_ATTRIBUTE_NODE,
                                                    List);
        assert( node != NULL);
        PATTR_RECORD pAttrRecord = (PATTR_RECORD)node->AttributeData;
        if( pAttrRecord->non_resident == 0 )
            continue;

        dataruns = (LPBYTE)((DWORD_PTR)pAttrRecord + pAttrRecord->mapping_pairs_offset);
        len_dataruns = node->Length - pAttrRecord->mapping_pairs_offset;
        LONGLONG startLcn = 0,len = 0;

//===================================================================================          
        //判断dataruns描述的簇表是否在坏扇区域内

        BOOL bFound = FALSE;
        for(DWORD i = 0;
            i < len_dataruns;
            )
        {
            DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

            if( dataruns[i] == 0 )break;

            cStartLcnBytes = ( dataruns[i] & 0xf0) >> 4;//压缩字节的高4位
            cLenLcnBytes = (dataruns[i] & 0x0f);        //压缩字节的低4位

            if( cStartLcnBytes != 0)
            {
                //提取当前数据流长度
                len = 0;
                for( DWORD j = cLenLcnBytes;j > 0;j--)
                {
                    len = (len << 8) | dataruns[i + j ]; 
                }

                //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
                LONGLONG tmp = 0;
                if( dataruns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                    tmp = -1ll;
                for( DWORD j = cStartLcnBytes;j > 0;j-- )
                {
                    tmp = ( tmp << 8 ) | dataruns[i + cLenLcnBytes + j ];
                }
                startLcn = startLcn + tmp;
                assert( startLcn >= 0 );
                
                for( PLIST_ENTRY list2 = m_BlockInforHead.BadBlockList.Flink;
                    list2 != &m_BlockInforHead.BadBlockList;
                    list2 = list2->Flink )
                {
                    PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list2,
                                                            BLOCK_DESCRIPTOR,
                                                            List );
                    assert( node != NULL);

                    LONGLONG startLcn2 = node->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster;
                    LONGLONG len2 = node->TotalSectors.QuadPart / m_BootSect.bpb.sectors_per_cluster;

                    if( (startLcn < startLcn2 + len2) &&
                        ( startLcn2 < startLcn + len ))
                    {
                        bFound = TRUE;
                        result = 1;
                        break;
                    }
                }//end for list2

                if( bFound )break;
            }//end if cStartLcn > 0 

            i += cStartLcnBytes + cLenLcnBytes + 1;
        }//end for i

//====================================================================================

        if( bFound  )
        {
            //目前无法处理压缩文件
            if( pAttrRecord->compression_unit != 0 )
            {
                result = 2;
                goto exit;
            }

            LONGLONG vcns = GetNumberOfVcnsInDataRuns( dataruns,(DWORD)len_dataruns );
            assert( vcns > 0 );
            //printf("vcns = %lld\n",vcns );
            LONGLONG newBlock = AllocateBlock( vcns );
            //printf("newBlock = %lld\n",newBlock );
            if( newBlock < 0 || CompressLongLong(newBlock)+CompressLongLong( vcns) + 2 > len_dataruns )                          //有待更新的地方
            {
                if( newBlock >= 0 )
                    FreeBlock( newBlock,vcns );
                result = 2;
                goto exit;
            }

//====================================================================================
            BOOL bOk=TRUE;
            CHAR message_buf[1024]={0};
            sprintf_s( message_buf,1024,"正在移动文件%lld ...   ",FileId);
            ReportStateMessage( message_buf);
            LONGLONG dstLcn = newBlock;
            startLcn = 0;

            for(DWORD i = 0;
                i < len_dataruns;
                )
            {
                DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

                if( dataruns[i] == 0 )break;

                cStartLcnBytes = ( dataruns[i] & 0xf0) >> 4;//压缩字节的高4位
                cLenLcnBytes = (dataruns[i] & 0x0f);        //压缩字节的低4位

                if( cStartLcnBytes != 0)
                {
                    //提取当前数据流长度
                    len = 0;
                    for( DWORD j = cLenLcnBytes;j > 0;j--)
                    {
                        len = (len << 8) | dataruns[i + j ]; 
                    }

                    //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
                    LONGLONG tmp = 0;
                    if( dataruns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                        tmp = -1ll;
                    for( DWORD j = cStartLcnBytes;j > 0;j-- )
                    {
                        tmp = ( tmp << 8 ) | dataruns[i + cLenLcnBytes + j ];
                    }
                    startLcn = startLcn + tmp;
                    assert( startLcn >= 0 );
                    bOk = CopyLogicalClusterBlock( startLcn,dstLcn,len );
                    if( !bOk )
                    {
                        FreeBlock( newBlock,vcns );
                        result = 2;
                        goto exit;
                    }
                    dstLcn += len;
                    ReportNotifyMessage();

                }//end if cStartLcn > 0 

                i += cStartLcnBytes + cLenLcnBytes + 1;
            }//end for i

//===============================================================================

            //释放原始dataruns中的可用空间
            startLcn = 0;len = 0;
            for(DWORD i = 0;
                i < len_dataruns;
               )
            {
                DWORD   cStartLcnBytes = 0,cLenLcnBytes = 0;

                if( dataruns[i] == 0 )break;

                cStartLcnBytes = ( dataruns[i] & 0xf0) >> 4;//压缩字节的高4位
                cLenLcnBytes = (dataruns[i] & 0x0f);        //压缩字节的低4位

                if( cStartLcnBytes != 0)
                {
                    //提取当前数据流长度
                    len = 0;
                    for( DWORD j = cLenLcnBytes;j > 0;j--)
                    {
                        len = (len << 8) | dataruns[i + j ]; 
                    }

                    //提取当前数据流起始簇号（可能是相对上一个数据流的偏移,有符号）
                    LONGLONG tmp = 0;
                    if( dataruns[i + cLenLcnBytes + cStartLcnBytes ] & 0x80 )
                        tmp = -1ll;
                    for( DWORD j = cStartLcnBytes;j > 0;j-- )
                    {
                        tmp = ( tmp << 8 ) | dataruns[i + cLenLcnBytes + j ];
                    }
                    startLcn = startLcn + tmp;
                    assert( startLcn >= 0 );
                    FreeBlock( startLcn,len );
#if 0
                    for( PLIST_ENTRY list2 = m_BlockInforHead.BadBlockList.Flink;
                        list2 != &m_BlockInforHead.BadBlockList;
                        list2 = list2->Flink )
                    {
                        PBLOCK_DESCRIPTOR node = (PBLOCK_DESCRIPTOR)CONTAINING_RECORD( list2,
                                                                BLOCK_DESCRIPTOR,
                                                                List );
                        assert( node != NULL);

                        LONGLONG startLcn2 = node->StartSector.QuadPart / m_BootSect.bpb.sectors_per_cluster;
                        LONGLONG len2 = node->TotalSectors.QuadPart / m_BootSect.bpb.sectors_per_cluster;
                        
                        if( (startLcn < startLcn2 + len2) &&
                            ( startLcn2 < startLcn + len ))
                        {
                            //有重叠的情况

                            LONGLONG overlappedStart
                                = max( startLcn,startLcn2 );
                            LONGLONG overlappedLen = 
                                min( startLcn + len,startLcn2 + len2)  - overlappedStart;

                            if( startLcn < overlappedStart )
                                FreeBlock( startLcn,overlappedStart - startLcn );
                            if( startLcn + len > overlappedStart + overlappedLen )
                                FreeBlock( overlappedStart + overlappedLen,
                                            (startLcn + len )-(overlappedStart + overlappedLen));
                        }
                    }//end for list2
#endif
                }//end if cStartLcn > 0;

                i += cStartLcnBytes + cLenLcnBytes + 1;
            }//end for i
//=================================================================================

            //修改属性中相应的dataruns指向newBlock

            LPBYTE buffer = (LPBYTE)malloc( this->m_MftRecordLength );
            assert( buffer != NULL);
            bOk = ReadMftRecord( node->OwnerRecordId,buffer,m_MftRecordLength );
            assert( bOk );
            PMFT_RECORD pRecordHeader = (PMFT_RECORD)buffer;
            
            PWORD   pUsa = (PWORD)((DWORD_PTR)pRecordHeader + pRecordHeader->usa_ofs);
            WORD    cUsa = pRecordHeader->usa_count;

            //用USA数组更新扇区末尾两个字节的数据
            for( WORD i = 1;i < cUsa;i++)
            {
                assert( *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) == pUsa[0]);
                *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[i];
            }

            PATTR_RECORD pAttrRecord2 = (PATTR_RECORD)((DWORD_PTR)pRecordHeader + pRecordHeader->attrs_offset);
            for( ;
                pAttrRecord2->type != AT_END ;
                pAttrRecord2 = (PATTR_RECORD)((DWORD_PTR)pAttrRecord2 + pAttrRecord2->length))
            {
                if( pAttrRecord->type == pAttrRecord2->type
                    && pAttrRecord->resident_flags == pAttrRecord2->resident_flags
                    && pAttrRecord->name_length == pAttrRecord2->name_length
                    && pAttrRecord->instance == pAttrRecord2->instance
                    && pAttrRecord->data_size == pAttrRecord2->data_size
                    && pAttrRecord->lowest_vcn == pAttrRecord2->lowest_vcn)
                    break;
            }
            assert( pAttrRecord2->type != AT_END );
            LPBYTE p = (LPBYTE)((DWORD_PTR)pAttrRecord2 + pAttrRecord2->mapping_pairs_offset);

            //计算newBlock和vcns的字节长度

            BYTE c_newBlock = 8,c_vcns = 8;
            c_newBlock = CompressLongLong( newBlock );
            c_vcns = CompressLongLong( vcns );
            //printf("c_newBlock = %d,c_vcns = %d\n",c_newBlock,c_vcns );

            p[0] = (c_newBlock << 4) | c_vcns;
            //printf("p[0] = 0x%.2x\n",p[0] );
            for( BYTE i = 0;i < c_vcns;i++)
            {
                p[i+1] = (BYTE)((vcns >> i*8) & 0xff);
                //printf(" 0x%.2x",p[i+1]);
            }
            for( BYTE i = 0;i < c_newBlock;i++)
            {
                p[c_vcns+i+1] = (BYTE)((newBlock >> i*8) & 0xff);
                //printf(" 0x%.2x",p[c_vcns+i+1]);
            }
            p[c_vcns + c_newBlock + 1] = 0;

            //处理特殊文件$MFT
            if( node->OwnerRecordId == FILE_MFT && 
                pAttrRecord2->name_length == 0 &&
                pAttrRecord2->type == AT_DATA )
            {
                memcpy_s(   m_MftDataRuns,
                            m_MftDataRunsLength,
                            p,
                            c_vcns + c_newBlock + 2 );
                m_BootSect.mft_lcn = newBlock;
            }

            //处理特殊文件$MFTMirr
            if( node->OwnerRecordId == FILE_MFTMirr && 
                pAttrRecord2->name_length == 0 &&
                pAttrRecord2->type == AT_DATA )
            {
                m_BootSect.mftmirr_lcn = newBlock;
            }

            for( WORD i = 1;i < cUsa;i++)
            {
                pUsa[i] = *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD));
                *(PWORD)((DWORD_PTR)pRecordHeader + i * m_BootSect.bpb.bytes_per_sector - sizeof(WORD)) = pUsa[0];
            }

            bOk = WriteMftRecord( FileId,buffer,m_MftRecordLength );
            assert( bOk );
            free( buffer);buffer = NULL;

//=================================================================================

        }//end if result == 1

    }//end for attribute list

exit:
    if( result == 2 )
    {
        WCHAR filename[1024]={0};
        memcpy_s( filename,1024*sizeof(WCHAR),hFile->FileName,hFile->FileNameLength*sizeof(WCHAR));
        size_t len = wcslen( filename );
        filename[ len ]=L'\r';
        filename[ len + 1] = L'\n';
        ReportFileNameMessage( filename );
    }

    if( hFile != NULL)
        CloseFile( hFile );

    return result;

//这个函数太长了。待更新
}


