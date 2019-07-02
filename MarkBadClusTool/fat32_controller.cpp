//fat32_controller.cpp for class-- CFat32Controller
//author:lzc
//date:2013/02/11
//e-mail:hackerlzc@126.com

#include "stdafx.h"
#include<windows.h>
#include"fat32_controller.h"

//¿‡ µœ÷

CFat32Controller::CFat32Controller(LPSTR lpszDiskPath, LONGLONG StartSector, LONGLONG NumberOfSectors)
:CRepairController( lpszDiskPath,StartSector,NumberOfSectors )
{
}

CFat32Controller::~CFat32Controller()
{
}

VOID CFat32Controller::PrepareUpdateBadBlockList()
{
}

VOID CFat32Controller::AddBadBlock(LONGLONG StartLsn, LONGLONG NumberOfSectors)
{
}

VOID CFat32Controller::AddDeadBlock(LONGLONG StartLsn, LONGLONG NumberOfSectors)
{
}

BOOL CFat32Controller::ProbeForRepair()
{
    return TRUE;
}

BOOL CFat32Controller::VerifyFileSystem()
{
    return TRUE;
}

BOOL CFat32Controller::StartRepairProgress()
{
    return TRUE;
}

BOOL CFat32Controller::StopRepairProgress()
{
    return TRUE;
}

BOOL CFat32Controller::InitController()
{
    return TRUE;
}

VOID CFat32Controller::ReleaseResources()
{
}







