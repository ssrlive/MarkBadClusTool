// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//
//e-mail:hackerlzc@126.com

#pragma once


#define WIN32_LEAN_AND_MEAN		// 从 Windows 头中排除极少使用的资料

#if !defined(NDEBUG)
#define MEM_LEAK_CHECK  1       //程序退出时检测内存泄漏
#endif

#include <stdio.h>
#include<stdlib.h>
#include <tchar.h>
#include<windows.h>
#include<assert.h>


// TODO: 在此处引用程序需要的其他头文件

#define MAINVERSION 0
#define SUBVERSION 1
