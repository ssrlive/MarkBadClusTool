//disk_list.h for class CDiskList
//author:lzc
//date:2012/11/07
//e-mail:hackerlzc@126.com

#pragma once
#ifndef _DISK_LIST_H
#define _DISK_LIST_H
#pragma warning(push)
#pragma warning(disable:4200)

#include<windows.h>
#include"utils.h"

#ifdef _UNICODE
#error this class not support UNICODE!
#endif

#define REG_ROOT    ("SYSTEM\\CurrentControlSet\\services\\Disk\\Enum")

typedef struct _DISK_DEVICE
{
    LIST_ENTRY  list;               //链表，用于磁盘设备表
    WORD        index;              //本结点所描述磁盘设备的序号（从0开始）
    WORD        reserved;           //保留
    LARGE_INTEGER sizeInSectors;    //扇区数表示的容量
    LARGE_INTEGER sectorsPerCylinder;//每柱面扇区数
    BYTE        *name;              //系统对磁盘的描述信息
    BYTE        *path;              //磁盘设备路径，用于打开磁盘设备
}DISK_DEVICE,*PDISK_DEVICE;

//注意：不支持多线程访问

class CDiskList:public CUtils
{
private:
    WORD        m_DiskCount;         //本机磁盘数量
    LIST_ENTRY  m_DiskListHead;     //磁盘信息链表头

public:
    CDiskList();                    //构造函数
    ~CDiskList();                   //柝构函数，释放资源
    WORD GetDiskCount();
    BOOL GetDiskByIndex( IN WORD index,OUT PDISK_DEVICE *result );
    PDISK_DEVICE GetFirstDisk();
    PDISK_DEVICE GetNextDisk( IN PDISK_DEVICE curDisk );
    VOID UpdateDiskList();

private:
    VOID ReleaseAllResources();
    VOID InitDiskList();
};

#pragma warning(pop)
#endif