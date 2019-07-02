//utils.h for class-CUtils
//author:lzc
//date:2012/11/07
//e-mail:hackerlzc@126.com

#pragma once
#ifndef _UTILS_H
#define _UTILS_H

#ifdef _DEBUG
#define DbgPrint(x) _DbgPrint( __FILE__,__FUNCTION__,x )
#else
#define DbgPrint(x) 
#endif
#define IOCTL_DISK_COPY_DATA    CTL_CODE(IOCTL_DISK_BASE, 0x0019, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _DISK_COPY_DATA_PARAMETERS {
  LARGE_INTEGER  SourceOffset;
  LARGE_INTEGER  DestinationOffset;
  LARGE_INTEGER  CopyLength;
  ULONGLONG      Reserved;
} DISK_COPY_DATA_PARAMETERS, *PDISK_COPY_DATA_PARAMETERS;


class CUtils
{
protected:
    CUtils();
    VOID InitializeListHead(OUT PLIST_ENTRY ListHead);
    VOID InsertTailList(IN PLIST_ENTRY  ListHead,
                                IN PLIST_ENTRY  Entry);
    VOID InsertHeadList(IN PLIST_ENTRY  ListHead,
                                IN PLIST_ENTRY  Entry);
    BOOL IsListEmpty( IN PLIST_ENTRY ListHead );
    PLIST_ENTRY RemoveHeadList(IN PLIST_ENTRY  ListHead);
    PLIST_ENTRY RemoveTailList(IN PLIST_ENTRY  ListHead);
    VOID RemoveEntryList( IN PLIST_ENTRY ListEntry );
    PLIST_ENTRY GetFirstListEntry( PLIST_ENTRY ListHead );
    PLIST_ENTRY GetNextListEntry( PLIST_ENTRY CurrEntry );
    BYTE CompressLongLong( LONGLONG x,BOOL bSigned=TRUE );
    VOID _DbgPrint(IN LPSTR file,IN LPSTR function,IN LPSTR message);
    VOID ShowError( IN DWORD ErrorCode );
    BOOL ReadSector( IN HANDLE hDisk,
                     OUT LPVOID buffer,
                     IN DWORD bufferSize,
                     IN DWORD SectorNumberLow,
                     IN DWORD SectorNumberHigh = 0
                     );
    BOOL WriteSector( IN HANDLE hDisk,
                     IN LPVOID buffer,
                     IN DWORD bufferSize,
                     IN DWORD SectorNumberLow,
                     IN DWORD SectorNumberHigh = 0
                     );
    BOOL CopyBlock( IN HANDLE hDisk,
                    IN ULONGLONG SourceSector,
                    IN ULONGLONG DestinationSector,
                    IN ULONGLONG NumberOfSectors );
};

#endif