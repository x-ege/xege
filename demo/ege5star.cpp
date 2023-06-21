// 旋转五角星动画演示程序
#include <graphics.h>
//#include <ege/fps.h>
#include <time.h>
#include <math.h>

const double rotatingSpeed = -0.003, fullCircleRatation = PI * 2, starAngle = PI * 4 / 5;

void paintstar(double x, double y, double r, double a)
{
	int pt[10];
	for (int n = 0; n < 5; ++n)
	{
		pt[n*2] = (int)( -cos( starAngle * n + a ) * r + x );
		pt[n*2+1] = (int)( sin( starAngle * n + a) * r + y );
	}
	fillpoly(5, pt);
}

int main()
{
	initgraph( 640, 480 );
	setrendermode(RENDER_MANUAL);
	double r = 0;
//	fps f;
	for ( ; is_run(); delay_fps(1000) )
	{
		r += rotatingSpeed;
		if (r > fullCircleRatation) r -= fullCircleRatation;
		
		cleardevice();
		setcolor( EGERGB(0xff, 0xff, 0xff) );
		setfillcolor( EGERGB(0, 0, 0xff) );
		paintstar(300, 200, 100, r);
	}
	return 0;
}
