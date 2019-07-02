//fat32_controller.h for class ----  CFat32Controller
//author:lzc
//date:2013/02/11
//e-mail:hackerlzc@126.com

#pragma once
#ifndef _FAT32_CONTROLLER_H
#define _FAT32_CONTROLLER_H
#pragma warning(push)
#pragma warning(disable:4200)

#include<windows.h>
#include"utils.h"
#include"repair_controller.h"

#ifdef _UNICODE
#error this class not support UNICODE!
#endif

//注意：不支持多线程

class CFat32Controller:public CRepairController
{
private:

protected:
    //类成员变量

public:
    //公共接口函数
    CFat32Controller( LPSTR lpszDiskPath,LONGLONG StartSector,LONGLONG NumberOfSectors);
    virtual ~CFat32Controller();
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
};

#pragma warning(pop)
#endif  //_FAT32_CONTROLLER_H