#define SHOW_CONSOLE
#include <iostream>
#include <graphics.h>					//包含EGE的头文件

int main()
{
	std::cout << "ege" << std::endl;
	initgraph(640, 480);				//初始化图形界面

	getch();
	closegraph();						//关闭图形界面

	return 0;
}