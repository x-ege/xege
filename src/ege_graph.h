#pragma once

#include "ege_common.h"


#ifndef _MSC_VER
#define GRADIENT_FILL_RECT_H   0x00000000
#define GRADIENT_FILL_RECT_V   0x00000001
#define GRADIENT_FILL_TRIANGLE 0x00000002
#define GRADIENT_FILL_OP_FLAG  0x000000ff

extern "C"
{
WINGDIAPI BOOL WINAPI GradientFill(HDC hdc, PTRIVERTEX pVertex, ULONG nVertex, PVOID pMesh, ULONG nMesh, ULONG ulMode);
}
#endif

namespace ege
{

extern struct _graph_setting graph_setting;
class egeControlBase;   // 前置声明

int dealmessage(_graph_setting* pg, bool force_update);

void guiupdate(_graph_setting* pg, egeControlBase* root);

int waitdealmessage(_graph_setting* pg);

float EGE_PRIVATE_GetFPS(int add); // 获取帧数

void setmode(int gdriver, int gmode);

// GDI+ 初始化
void gdipluinit();
}
