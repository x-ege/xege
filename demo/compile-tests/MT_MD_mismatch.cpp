#define SHOW_CONSOLE
#include <iostream>
#include <graphics.h>					//����EGE��ͷ�ļ�

int main()
{
	std::cout << "ege" << std::endl;
	initgraph(640, 480);				//��ʼ��ͼ�ν���

	getch();
	closegraph();						//�ر�ͼ�ν���

	return 0;
}