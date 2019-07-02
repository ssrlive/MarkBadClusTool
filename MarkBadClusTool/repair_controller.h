//repair_controller.h for class ----  CRepairController
//author:lzc
//date:2012/11/14
//e-mail:hackerlzc@126.com

#pragma once
#ifndef _REPAIR_CONTROLLER_H
#define _REPAIR_CONTROLLER_H
#pragma warning(push)
#pragma warning(disable:4200)

#include<windows.h>
#include"utils.h"

#ifdef _UNICODE
#error this class not support UNICODE!
#endif

//注意：不支持多线程

#define BLOCK_TYPE_UNKNOWN      0x00
#define BLOCK_TYPE_USED         0x01
#define BLOCK_TYPE_FREE         0x02
#define BLOCK_TYPE_BAD          0x03
#define BLOCK_TYPE_MAXIMUM      0xff

typedef struct _BLOCK_DESCRIPTOR
{
    LIST_ENTRY  List;
    BYTE        type;               //BLOCK_TYPE_XXX
    BYTE        reserved1[3];       //保留
    LARGE_INTEGER StartSector;      //起始扇区号
    LARGE_INTEGER TotalSectors;     //扇区总数
}BLOCK_DESCRIPTOR,*PBLOCK_DESCRIPTOR;

typedef struct _BLOCK_INFOR_HEAD
{
    LARGE_INTEGER UsedBlockSize;    //单位：字节
    LIST_ENTRY  UsedBlockList;      //由特定的文件系统模块建立和释放

    LARGE_INTEGER FreeBlockSize;    //单位：字节
    LIST_ENTRY  FreeBlockList;      //由特定的文件系统模块建立和释放

    LARGE_INTEGER BadBlockSize;     //单位：字节
    LIST_ENTRY  BadBlockList;       //由特定的文件系统模块建立和释放
    LIST_ENTRY  DeadBlockList;      //由UI通过特定的基类接口建立
}BLOCK_INFOR_HEAD,*PBLOCK_INFOR_HEAD;

#define MESSAGE_CODE_UNKNOWN    0x00 //未知通知码
#define MESSAGE_CODE_NOTIFY     0x01 //通知消息，用于进度条控制，Param1,2无定义
#define MESSAGE_CODE_REPORTSTATE 0x02 //状态报告通知码，Param1为一字符串指针
                                     //Param2未定义
#define MESSAGE_CODE_PROGRESS   0x03 //进度消息，Param1为当前处理完成的数据量，
                                     //Param2为总数据量
#define MESSAGE_CODE_REPORTERROR 0x04 //错误报告通知码，Param1为字符串指针
                                      //Param2未定义
#define MESSAGE_CODE_FILENAME 0x05    //报告文件名，Param1为宽字符串指针（LPWSTR),
#define MESSAGE_CODE_MAXIMUM    0xff


typedef VOID (*MESSAGE_CALLBACK_FUNC)(
                IN BYTE Code,
                IN OUT DWORD_PTR Param1,
                IN OUT DWORD_PTR Param2);

class CRepairController:public CUtils
{
protected:
    //类私有变量
    BLOCK_INFOR_HEAD m_BlockInforHead;
    LARGE_INTEGER    m_VolumeStartSector;
    LARGE_INTEGER    m_VolumeTotalSectors;
    HANDLE  m_hDisk;
    MESSAGE_CALLBACK_FUNC   m_lpMessageFunc;
public:
    //重写父类成员函数

public:
    //公共接口函数
    CRepairController( LPSTR lpszDiskPath,LONGLONG StartSector,LONGLONG NumberOfSectors);
    virtual ~CRepairController();
    PBLOCK_DESCRIPTOR GetFirstUsedBlock();
    PBLOCK_DESCRIPTOR GetNextUsedBlock( PBLOCK_DESCRIPTOR CurrBlock );
    PBLOCK_DESCRIPTOR GetFirstFreeBlock();
    PBLOCK_DESCRIPTOR GetNextFreeBlock( PBLOCK_DESCRIPTOR CurrBlock );
    PBLOCK_DESCRIPTOR GetFirstBadBlock();
    PBLOCK_DESCRIPTOR GetNextBadBlock( PBLOCK_DESCRIPTOR CurrBlock );
    LONGLONG GetUsedBlockSize();
    LONGLONG GetFreeBlockSize();
    LONGLONG GetBadBlockSize();
    MESSAGE_CALLBACK_FUNC RegisterMessageCallBack( MESSAGE_CALLBACK_FUNC lpFn );
    MESSAGE_CALLBACK_FUNC UnregisterMessageCallBack();
    VOID ReportStateMessage( LPSTR message );
    VOID ReportFileNameMessage(LPWSTR FileName );
    VOID ReportErrorMessage( LPSTR message );
    VOID ReportProgressState( DWORD Curr,DWORD Total );
    VOID ReportNotifyMessage();
    virtual VOID PrepareUpdateBadBlockList()=0;
    virtual VOID AddBadBlock( LONGLONG StartLsn,LONGLONG NumberOfSectors )=0;
    virtual VOID AddDeadBlock( LONGLONG StartLsn,LONGLONG NumberOfSectors )=0;
    virtual BOOL ProbeForRepair()=0;
    virtual BOOL VerifyFileSystem()=0;
    virtual BOOL StartRepairProgress() = 0;
    virtual BOOL StopRepairProgress() = 0;

protected:
    //可被继承的类私有函数
    BOOL ReadLogicalSector( OUT LPVOID buffer,
                     IN DWORD bufferSize,
                     LONGLONG Lsn
                     );
    BOOL WriteLogicalSector( IN LPVOID buffer,
                     IN DWORD bufferSize,
                     LONGLONG Lsn
                     );
    virtual BOOL InitController() = 0;//初始化Controller,不同的文件系统有不同的初始化方法，真正在子类中
                                      //实现此接口，用来实现对不同文件系统的支持。
    virtual VOID ReleaseResources();

private:
    //不可被继承的私有函数
};

#endif    //_REPAIR_CONTROLLER_H