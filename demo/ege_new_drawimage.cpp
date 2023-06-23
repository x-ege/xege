#include <graphics.h>

int main()
{
	// 初始化一个长800像素，宽600像素的EGE窗口
	initgraph( 800, 600 );
	// 绘制背景：背景为一个白底蓝条纹的矩形
	setbkcolor( WHITE ); // 在白色的基础上绘制
	setfillcolor( BLUE ); // 条纹填充颜色：蓝色
	
	for (int i=0;i<12;i++) { // 一共12个蓝色填充矩形作蓝条纹
		//void EGEAPI bar(int left, int top, int right, int bottom, PIMAGE pimg = NULL); 画无边框填充矩形
		bar(0,i*50,800,i*50+20);
	}
   
   	//image to be rotated and put
	PIMAGE img = newimage(200,200); //新建一个图像，newimage中有参数则需要指定图像大小（长像素数，宽像素数）
	setbkcolor(EGERGBA(0,0,0,0),img); //将背景设为透明色
	setcolor(RED,img); //将绘制边缘颜色设为红色
	setfillcolor(YELLOW,img); //图像填充颜色：黄色
	
	// 绘制一个多边形
	ege_point points[4];
	points[0].x=100;
	points[0].y=50;
	points[1].x=50;
	points[1].y=150;
	points[2].x=150;
	points[2].y=150;
	points[3].x=100;
	points[3].y=50;
	ege_fillpoly(3,points,img); //绘制填充多边形到目标图像（为NULL则绘制到屏幕）
	ege_drawpoly(4,points,img); //绘制非填充多边形到目标图像（为NULL则绘制到屏幕）
	
	/*
	  int EGEAPI putimage_withalpha(
	  PIMAGE imgdest,         // handle to dest
	  PCIMAGE imgsrc,         // handle to source
	  int nXOriginDest,       // x-coord of destination upper-left corner
	  int nYOriginDest,       // y-coord of destination upper-left corner
	  int nXOriginSrc = 0,    // x-coord of source upper-left corner
	  int nYOriginSrc = 0,    // y-coord of source upper-left corner
	  int nWidthSrc = 0,      // width of source rectangle
	  int nHeightSrc = 0      // height of source rectangle
	  );
	 */
	// 简而言之，将图像绘制到目标图像上（为NULL则绘制到屏幕），保留透明度信息
	putimage_withalpha(NULL,img,0,0);
	// 将图像的指定部分绘制到屏幕，从(200,0)点开始绘制，到(400,300) 点结束
	ege_drawimage(img,200,0,400,300,0,0,200,200);

	// 变换矩阵
	ege_transform_matrix m;
	// 通过变换矩阵来绘制图像
	ege_get_transform(&m);
	ege_transform_translate(400,450); //窗口下半部分的中心，读者可以自己算一下是否为此坐标点
	ege_transform_rotate(45.0); // 旋转45度
	ege_transform_translate(-getwidth(img)/2, -getheight(img)/2); // 目标图像的中心
	ege_drawimage(img,0,0); //绘制到窗口
	// 恢复原来的
	ege_set_transform(&m);
	
	// 下面的语句可以清除矩阵
	//ege_transform_reset();
	
	// 绘制到窗口
	ege_drawimage(img,600,400);
	
	getch(); // 等待按键后才结束，防止还没看清就关闭窗口了
		
	delimage(img); // 使用完img后需要销毁
	closegraph(); // 程序结束后关闭窗口是个好习惯
	return 0;
}
