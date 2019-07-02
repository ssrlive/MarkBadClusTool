//volume_list.h for class-CVolumeList
//author:lzc
//date:2012/11/10
//e-mail:hackerlzc@126.com

#pragma once
#ifndef _VOLUME_LIST_H
#define _VOLUME_LIST_H

#pragma warning(push)
#pragma warning(disable:4200)

#include<windows.h>
#include"utils.h"

typedef struct _VOLUME_NODE
{
    LIST_ENTRY  List;               //链表，用于生成卷结点表
    DWORD       Index;              //卷序数（从0编号）
    BYTE        Type;               //卷类型代码
    CHAR        VolumeLetter;       //盘符
    BYTE        Reserved1[2];       //保留
    LARGE_INTEGER StartSector;      //卷在物理磁盘中起始位置的绝对扇区号
    DWORD       Reserved2;          //保留
    LARGE_INTEGER TotalSectors;     //卷所包括的扇区总数
    DWORD       Reserved3;          //保留
    LPSTR       VolumeName;         //指向卷名称的字符串
    LPSTR       TypeName;
}VOLUME_NODE,*PVOLUME_NODE;

class CVolumeList:public CUtils
{
private:
    WORD        m_VolumeCount;      //磁盘中卷数量
    LIST_ENTRY  m_VolumeListHead;   //卷结点链表头
    HANDLE      m_hDisk;
    DWORD       m_DiskId;
    DWORD       m_tbl_VolumeOwnerDiskId[26]; //分区所属磁盘号 映射表
    DWORDLONG   m_tbl_VolumeOffset[26]; //保存分区相对磁盘起始处的偏移(字节）映射表

public:
    CVolumeList( LPSTR DiskPath,DWORD DiskId);   //构造函数
    ~CVolumeList();                 //柝构函数，释放资源
    WORD GetVolumeCount();
    BOOL GetVolumeByIndex( IN WORD index,OUT PVOLUME_NODE *result );
    PVOLUME_NODE GetFirstVolume();
    PVOLUME_NODE GetNextVolume( IN PVOLUME_NODE curVolume );
    VOID UpdateVolumeList();

private:
    VOID ReleaseAllResources();
    VOID InitVolumeList();
    DWORD SearchMbrVolume( DWORD BaseSector,DWORD BaseEbrSector = 0 );
    BOOL IsVolumeTypeSupported( BYTE type );
};

#pragma warning(pop)
#endif