//ntfs_controller.h for class ----  CNtfsController
//author:lzc
//date:2012/11/14
//e-mail:hackerlzc@126.com

#pragma once
#ifndef _NTFS_CONTROLLER_H
#define _NTFS_CONTROLLER_H
#pragma warning(push)
#pragma warning(disable:4200)

#include<windows.h>
#include"utils.h"
#include"repair_controller.h"
#include"layout_ntfs.h"

#ifdef _UNICODE
#error this class not support UNICODE!
#endif

//注意：不支持多线程

typedef struct _FILE_ATTRIBUTE_NODE
{
    LIST_ENTRY      List;
    LONGLONG        OwnerRecordId;  //本属性所属的文件记录ID
    WORD            AttrOffset;     //本属性在文件记录中的偏移
    DWORD           AttributeType;  //属性类型
    LPVOID          AttributeData;  //指向属性数据的指针(该数据包括属性头在内）
    DWORD           Length;         //属性数据的长度
}FILE_ATTRIBUTE_NODE,*PFILE_ATTRIBUTE_NODE;

typedef struct _FILE_INFORMATION
{
    LIST_ENTRY      List;           //属性链表，元素为FILE_ATTRIBUTE_NODE
    LONGLONG        FileSize;       //文件大小（字节）
    PWCHAR          FileName;       //指向UNICODE文件名
    DWORD           FileNameLength; //文件名字符数
    LONGLONG        FileRecordId;   //文件记录号
}FILE_INFORMATION,*PFILE_INFORMATION;

typedef PFILE_INFORMATION NTFS_FILE;

class CNtfsController:public CRepairController
{
private:

    NTFS_BOOT_SECTOR m_BootSect;                //NTFS的DBR
    DWORD           m_ClusterSizeInBytes;       //簇字节大小
    DWORD           m_MftRecordLength;          //文件记录大小（单位字节）
    LPBYTE          m_MftDataRuns;              //指向$MFT文件80属性的Datarns(内存动态申请）
    DWORD           m_MftDataRunsLength;        //m_MftDataRuns指向的缓冲区大小
    LONGLONG        m_MftNumberOfRecord;        //$MFT文件中文件记录数
    LPBYTE          m_Bitmap;                   /*指向用于映射Bitmap的缓冲区，InitBitmap中开辟内存并初始化
                                                ReleaseAllResources中释放*/
    LONGLONG        m_BitmapLength;             //Bitmap实际大小（单位：字节）
    LPBYTE          m_MftBitmap;                //$MFT的Bitmap属性数据，ReleaseAllResources中释放
    LONGLONG        m_MftBitmapLength;          //m_MftBitmap的长度（字节）
protected:
    //类成员变量

public:
    //公共接口函数
    CNtfsController( LPSTR lpszDiskPath,LONGLONG StartSector,LONGLONG NumberOfSectors);
    virtual ~CNtfsController();
    virtual VOID PrepareUpdateBadBlockList();
    virtual VOID AddBadBlock( LONGLONG StartLsn,LONGLONG NumberOfSectors );
    virtual VOID AddDeadBlock( LONGLONG StartLsn,LONGLONG NumberOfSectors );
    virtual BOOL ProbeForRepair();
    virtual BOOL VerifyFileSystem();
    virtual BOOL StartRepairProgress();
    virtual BOOL StopRepairProgress();

protected:
    //可被继承的类私有函数
    virtual BOOL InitController();
    virtual VOID ReleaseResources();
private:
    //不可被继承的私有函数
    BOOL InitFreeAndUsedBlockList();
    VOID DestroyListNodes( PLIST_ENTRY ListHead);
    BOOL ReadLogicalCluster( OUT LPVOID Buffer,
                             IN DWORD BufferSize,
                             IN LONGLONG Lcn,
                             IN DWORD TryTime = 1,
                             IN BYTE BadByte = 0xbb);
    BOOL WriteLogicalCluster( IN LPVOID Buffer,
                             IN DWORD DataLen,
                             IN LONGLONG Lcn,
                             IN DWORD TryTime = 1);
    BOOL CopyLogicalClusterBlock( IN LONGLONG SourceLcn,
                           IN LONGLONG DestLcn,
                           IN LONGLONG NumberOfLcns );
    LONGLONG GetNumberOfVcnsInDataRuns( IN LPBYTE DataRuns,IN DWORD Length );
    LONGLONG GetLastStartLcnInDataruns( IN LPBYTE DataRuns,IN DWORD Length );
    DWORD    GetDataRunsLength( IN LPBYTE DataRuns,IN DWORD Length );
    LONGLONG GetDataRunsValue( IN LPBYTE DataRuns,IN DWORD Length,OUT LPVOID Buffer,IN LONGLONG BufferLength );
    LONGLONG VcnToLcn( LONGLONG Vcn,LPBYTE DataRuns,DWORD Length );
    LONG BlockListToDataRuns( IN PLIST_ENTRY BlockListHead,OUT LPBYTE Dataruns,IN LONGLONG DatarunsLength );
    LONG DataRunsToSpaceDataRuns(IN LPBYTE dataruns,IN LONGLONG len_dataruns,OUT LPBYTE bad_dataruns,IN LONGLONG len_bad_dataruns );
    BOOL InitMftFile( IN LPVOID MftRecordCluster,IN DWORD BufferLength );
    BOOL InitBitmap();
    BOOL InitMftBitmap();
    BOOL UpdateBitmap();
    BOOL UpdateMftBitmap();
    BOOL UpdateBadClus();
    BOOL UpdateMftMirr();
    BOOL InitBadBlockList();
    PFILE_INFORMATION InitNtfsFile( IN LPVOID MftRecordCluster,IN DWORD BufferLength,LONGLONG RecordId );
    PFILE_INFORMATION OpenNtfsFile( IN LONGLONG RecordId );
    BOOL ReadMftRecord( IN LONGLONG RecordId,OUT PVOID Buffer,IN DWORD BufferLength );
    BOOL WriteMftRecord( IN LONGLONG RecordId,IN PVOID Buffer,IN DWORD BufferLength );
    VOID CloseFile( IN PFILE_INFORMATION File );
    LONG GetAttributeListValue( IN PATTR_RECORD AttrRecord,
                                OUT PVOID Buffer,
                                IN DWORD BufferLength,
                                OUT PDWORD BytesReturned );
    LONG GetAttributeValue( IN NTFS_FILE File,
                             IN ATTR_TYPES Type,
                             OUT PVOID Buffer,
                             IN DWORD BufferLength,
                             OUT PBOOL IsDataruns = NULL,
                             IN PWCHAR AttrName = NULL,
                             IN WORD Instance = 0,
                             OUT PDWORD BytesReturned = NULL);
    VOID UpdateBitmapFromBlockList(PLIST_ENTRY ListHead);
    LONGLONG AllocateBlock( LONGLONG LengthInCluster );
    BOOL FreeBlock( LONGLONG StartCluster,LONGLONG LengthInCluster );
    BOOL FreeBlockInDataruns( IN LPBYTE Dataruns,DWORD Length);
    LONGLONG AllocateFileRecordId();
    VOID FreeFileRecordId( LONGLONG FileRecordId );
    VOID _ShowList();
    LONG CheckAndUpdateFile( IN LONGLONG FileId );
};

#pragma warning(pop)
#endif  //_NTFS_CONTROLLER_H