// GBox.cpp : Defines the entry point for the console application.
//
#include <windows.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "stdafx.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "..\units\StdGUI.h"
#include "resource.h"

// Статистика
double MaxZ = 0.0;
double MinZ = 1E6;
double MaxX = 0.0;
double MinX = 1E6;
double MaxY = 0.0;
double MinY = 1E6;
double WPath = 0.0;
double MPath = 0.0;

double MaxPath = 0;//5;


double gr = 0.10;



// Высоты:

double dz = 9.0;

double CoolZ = 40 + dz;//58;
double SafeZ = 64;//53.5 + dz;//58;//40;
double TopZ = 67;//55 + dz;//59.36;//42.6;
double BotZ = 71;//60 + dz;//59.36;//42.6;
double ArcH = 0.01;

double Pos[3] = {0.0};

double DX = 0, DY = 15;
double WorkWidth = 200, WorkHeight = 68;
double Path = 0;

bool GraphOut = true;

double AreaSize[3] = {320, 220, 100};

struct TLine
{
	double Pos[3];
	double V;
} * Lines = 0;
int LineCount = 0;

struct T2D
{
	double X, Y;
};

void AddLine(double X, double Y, double Z, double V = 0)
{
	if(!GraphOut) return;
	if(!(LineCount & 0xFFF))
		Lines = (TLine *) realloc(Lines, sizeof(TLine) * (LineCount + 0x1000));
	if(LineCount)
	{
		TLine * l = Lines + LineCount - 1;
		double dx = X - l->Pos[0], dy = Y - l->Pos[1], dz = Z - l->Pos[2];
		if(V) WPath += sqrt(dx * dx + dy * dz + dz * dz);
		else 
		{
			double hp = abs(dx);
			if(hp < abs(dy)) hp = abs(dy);
			if(hp < abs(dz)) hp = abs(dz);
			MPath += hp;
		}
	}
	if(MinZ > Z) MinZ = Z;
	if(MaxZ < Z) MaxZ = Z;
	Lines[LineCount].Pos[0] = X;
	Lines[LineCount].Pos[1] = Y;
	Lines[LineCount].Pos[2] = Z;
	Lines[LineCount].V = V;
	LineCount++;
}

void Convert(double &x, double &y, double &z)
{
	x += DX;
	y += DY;
	if(z < 0) z = SafeZ;
	else z = z * (BotZ - TopZ) + TopZ;
}

void LineTo(double x, double y, double z, bool conv = true, bool Drill = false)
{
	if(conv) Convert(x, y, z);
	AddLine(x, y, z, 1);
	if(Drill) printf("G0 Z%.3f", SafeZ);
	if(MaxPath)
	{
		double a = x - Pos[0], b = y - Pos[1], c = z - Pos[2];
		double l = sqrt(a * a + b * b + c * c);
		double k = (MaxPath - Path) / l;
		while(k < 1)
		{
			double A = Pos[0] + a * k;
			double B = Pos[1] + b * k;
			double C = Pos[2] + c * k;
			if(Drill)
				printf("G0 X%.3f Y%.3f Z%.3f\n", A, B, SafeZ);
			else
				printf("G1 X%.3f Y%.3f Z%.3f\n", A, B, C);
			k += MaxPath / l;
			if(Drill)
				printf("G1 Z%.3f G0 Z%.3f\n", C, SafeZ);
			else
				printf("G0 Z%.3f G1 Z%.3f\n", CoolZ, C);
		}
		Path = MaxPath - l * (k - 1);
	}
	if(Drill)
		printf("G0 X%.3f Y%.3f Z%.3f\n", x, y, SafeZ);
	else
		printf("G1 X%.3f Y%.3f Z%.3f\n", x, y, z);
	Pos[0] = x;
	Pos[1] = y;
	Pos[2] = z;
}

void MoveTo(double x, double y, double z)
{
	Convert(x, y, z);
	AddLine(x, y, z, 0);
	if(Pos[2] >= SafeZ) printf("G0 Z%.3f ", SafeZ);
	if(z >= SafeZ) printf("G0 X%.3f Y%.3f Z%.3f G1 Z%.3f\n", x, y, SafeZ, z);
	else printf("G0 X%.3f Y%.3f Z%.3f\n", x, y, z);
	Pos[0] = x;
	Pos[1] = y;
	Pos[2] = z;
}

void ArcTo(double x, double y, double z, double a, bool Drill = false) // По часовой
{
	Convert(x, y, z);
	double dx = (x - Pos[0]) * 0.5;
	double dy = (y - Pos[1]) * 0.5;
	double k = a * M_PI / 360; // Половина угла
	double t = tan(k);
	double cx = Pos[0] + dx + dy / t;
	double cy = Pos[1] + dy - dx / t;
	dx = Pos[0] - cx;
	dy = Pos[1] - cy;
	double r = sqrt(dx * dx + dy * dy); // Радиус
	double an = acos(1 - ArcH / r); // Половина минимального угла
	int c = (int) ceil(abs(k / an));// Число шагов
	an = 2 * k / c; // Угол шага
	double aa = cos(an), bb = sin(an);
	double Z = Pos[2], dZ = (z - Z) / c;
	for(int x = 0; x < c; x++)
	{
		double t = dx;
		dx = dx * aa + dy * bb;
		dy = dy * aa - t * bb;
		Z += dZ;
		LineTo(cx + dx, cy + dy, Z, false, Drill);
	}
}

void SolveArc(double &cx, double &cy, double r, double x1, double y1, double x2, double y2)
{
	double x = x2 - x1, y = y2 - y1;
	double L2 = x * x + y * y;
	double xa = x1 - cx, ya = y1 - cy;
	double sm = (x * xa + y * ya) / L2;
	xa -= x * sm; ya -= y * sm; // OP
	sm = sqrt((r * r - xa * xa - ya * ya) / L2);
	cx += xa; cy += ya;
	double mx = (x1 + x2) * 0.5 - cx, my = (y1 + y2) * 0.5 - cy;
	if(mx * x + my * y < 0) sm = -sm;
	cx += x * sm, cy += y * sm;
}

StdGUI::TImage * I = 0;
double Ik, Ix = 0, Iy = 0;
double VAngle = 0, HAngle = 0;
double MulXX = 1.0, MulXY = 0.0, MulXZ = 0.0, MulYY = 1.0, MulYX = 0.0, MulYZ = 0.0;

bool ShowWork = true, ShowMove = true, Show3D = false;
REAL height;

void GetCoord(REAL &x, REAL &y, const TLine * P)
{
	x = (REAL)((P->Pos[0] * MulXX + P->Pos[1] * MulXY + P->Pos[2] * MulXZ + Ix) * Ik);
	y = (REAL)(height - (P->Pos[0] * MulYX + P->Pos[1] * MulYY + P->Pos[2] * MulYZ + Iy) * Ik);
}

void DrawImage(void)
{
	I->Fill(0xFFFFFF);
	height = (REAL)I->GetHeight();
	Graphics graphics(I->GetBMP());
	graphics.SetSmoothingMode(SmoothingModeHighQuality);//SmoothingModeAntiAlias8x8;
	Pen blue(Color(255, 0, 0, 255));
	Pen red(Color(255, 255, 0, 0));
	Pen black(Color(255, 0, 0, 0));
	graphics.DrawRectangle(&black, (REAL)(Ix * Ik), (REAL)(height - (Iy + AreaSize[1]) * Ik), (REAL)(AreaSize[0] * Ik), (REAL)(AreaSize[1] * Ik));
	graphics.DrawRectangle(&black, (REAL) ((DX + Ix) * Ik), (REAL) (height - (DY + Iy + WorkHeight) * Ik), (REAL)(WorkWidth * Ik), (REAL)(WorkHeight * Ik));
	//int P[2] = {0, 0};
	REAL xx, yy, aa, bb;
	GetCoord(aa, bb, Lines);
	if(ShowMove || ShowWork) for(int x = 1; x < LineCount; x++)
	{
		GetCoord(xx, yy, Lines + x);
		if(Lines[x].V)
		{
			if(ShowWork)
				graphics.DrawLine(&red, xx, yy, aa, bb);
		} else {
			if(ShowMove)
				graphics.DrawLine(&blue, xx, yy, aa, bb);//(REAL)((Lines[x - 1].Pos[0] + Ix) * Ik), (REAL)(height - (Lines[x - 1].Pos[1] + Iy) * Ik), (REAL)((Lines[x].Pos[0] + Ix) * Ik), (REAL)(height - (Lines[x].Pos[1] + Iy) * Ik));
		}
		aa = xx; bb = yy;
	}
	I->Update();
}

void OnMouseEvent(StdGUI::TImage * Image, int x, int y, int mk, UINT uMsg)
{
	static int X, Y, Xs, Ys;
	switch(uMsg)
	{
	case WM_RBUTTONDOWN:
		Xs = x;
		Ys = y;
		break;
	case WM_MOUSEWHEEL:
		{
			if(!(mk >>= 16)) return;
			double h = Image->GetHeight();
			Ix -= x / Ik;
			Iy -= (h - y) / Ik;
			Ik *= (mk > 0) ? 1.2 : (1 / 1.2);
			Ix += x / Ik;
			Iy += (h - y) / Ik;
			DrawImage();
		}
		break;
	case WM_MOUSEMOVE:
		if(mk & MK_RBUTTON)
		{
			if(Show3D)
			{
				HAngle += (Y - y) * 0.001;
				VAngle += (X - x) * 0.001;
				MulYY = MulXX = cos(VAngle);
				MulYX = -(MulXY = sin(VAngle));
				MulYY *= cos(HAngle);
				MulYX *= cos(HAngle);
				MulYZ = sin(HAngle);
			}
			else
			{
				double h = Image->GetHeight();
				Ix -= Xs / Ik;
				Iy -= (h - Ys) / Ik;
				Ik *= exp((Y - y) * 0.02);
				Ix += Xs / Ik;
				Iy += (h - Ys) / Ik;
			}
		}
		if(mk & MK_LBUTTON)
		{
			Ix -= (X - x) / Ik;
			Iy += (Y - y) / Ik;
		}
		X = x;
		Y = y;
		if(mk & (MK_LBUTTON | MK_RBUTTON))	DrawImage();
		break;
	}
}

HMENU Menu;

void OnWorkShow(void)
{
	int s = GetMenuState(Menu, ID_40001, MF_BYCOMMAND);
	ShowWork = (s & MF_CHECKED) ? false : true;
	CheckMenuItem(Menu, ID_40001, MF_BYCOMMAND | (ShowWork ? MF_CHECKED : MF_UNCHECKED));
	DrawImage();
}

void OnMoveShow(void)
{
	int s = GetMenuState(Menu, ID_40002, MF_BYCOMMAND);
	ShowMove = (s & MF_CHECKED) ? false : true;
	CheckMenuItem(Menu, ID_40002, MF_BYCOMMAND | (ShowMove ? MF_CHECKED : MF_UNCHECKED));
	DrawImage();
}

void On3D(void)
{
	int s = GetMenuState(Menu, ID_40004, MF_BYCOMMAND);
	Show3D = (s & MF_CHECKED) ? false : true;
	CheckMenuItem(Menu, ID_40004, MF_BYCOMMAND | (Show3D ? MF_CHECKED : MF_UNCHECKED));
	if(s) DrawImage();
}

void ShowLines(void)
{
	TCHAR b[128];
	_stprintf_s(b, sizeof(b) / sizeof(TCHAR), _T("GBox - WP = %.2fмм, MP = %.2fмм, MinZ = %.2fмм, MaxZ = %.2fмм"), WPath, MPath, MinZ, MaxZ);

	HMODULE hInst = GetModuleHandle(0);
	Menu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU1));
	StdGUI::TAppWindow * W = new StdGUI::TAppWindow(hInst, b, Menu);
	W->Commands(ID_40001)->Assign(&OnWorkShow);
	W->Commands(ID_40002)->Assign(&OnMoveShow);
	W->Commands(ID_40004)->Assign(&On3D);
	I = new StdGUI::TImage(W, 0, LT_ALCLIENT);
	Ik = I->GetWidth() / AreaSize[0];
	REAL h = (REAL)I->GetHeight();
	double kv = h / AreaSize[1];
	if(Ik > kv) Ik = kv;
	Ik *= 0.95;
	I->SetBitMapSize(0, 0, 32);
	//I->Fill(0xFFFFFF);
	I->OnMouse = &OnMouseEvent;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	{
		DrawImage();
		W->Show(SW_SHOWNORMAL);
		StdGUI::DefMessageLoop(0);
		GdiplusShutdown(gdiplusToken);
	}
}

struct CMat
{
	double M[4];
	double V[2];
	double norm;
	unsigned char TTL;
	inline CMat & operator *= (const CMat & a)
	{
		V[0] += M[0] * a.V[0] + M[1] * a.V[1];
		V[1] += M[2] * a.V[0] + M[3] * a.V[1];
		double t = M[0];
		M[0] = M[0] * a.M[0] + M[1] * a.M[2];
		M[1] = t    * a.M[1] + M[1] * a.M[3];
		t = M[2];
		M[2] = M[2] * a.M[0] + M[3] * a.M[2];
		M[3] = t    * a.M[1] + M[3] * a.M[3];
		TTL -= a.TTL;
		norm *= a.norm;
		return *this;
	}
	inline CMat & operator * (const CMat & a)
	{
		CMat R;
		R.V[0] = V[0] + M[0] * a.V[0] + M[1] * a.V[1];
		R.V[1] = V[1] + M[2] * a.V[0] + M[3] * a.V[1];
		R.M[0] = M[0] * a.M[0] + M[1] * a.M[2];
		R.M[1] = M[0] * a.M[1] + M[1] * a.M[3];
		R.M[2] = M[2] * a.M[0] + M[3] * a.M[2];
		R.M[3] = M[2] * a.M[1] + M[3] * a.M[3];
		R.norm = norm * a.norm;
		R.TTL = TTL - a.TTL;
		return *this;
	}
	inline void InitNorm(void) 
	{
		//return sqrt(M[0] * M[0] + M[1] * M[1] + M[2] * M[2] + M[3] * M[3]);
		norm = sqrt(abs(M[0] * M[3] - M[1] * M[2]));
	};
	inline void Init(double Length, double Angle, double P = 1.0)
	{
		Angle *= M_PI / 180;
		double a = cos(Angle), b = sin(Angle);
		M[0] = Length * a;
		M[2] = Length * b;
		M[1] = -Length * b * P;
		M[3] = Length * a * P;
		InitNorm();
	}
	//CMat(double a, double b, double c, double d, double x, double y) {M[};
};
/*
	Фракталы

	Управляющие символы:
	m: MoveTo, l: LineTo, a: ArcTo
	#число - установить TTL
	0 - 9: Применить матрицу с соответствующим номером
	[ - начать новую последовательность, запомнить текущую матрицу в стеке
	] - завершить последовательность, восстановить матрицу.

	Последовательности должны выполняться раздельно.

*/
//char * Codes[] = {"AmS", "S0lS[1A][2A]", 0};
//char * Codes[] = {"AmS", "S0l[5S][6S]", "Ll[3L][4L]", 0};//
//char * Codes[] = {"AmL", "S[1L][2L]\0[1A][2A]", "Ll[3L]\0[4L]", 0};
//char * Codes[] = {"A0mS", "S1 5aS[2B]1mC", "Bm#9F10B", "C[6lC][m7C][m8C]", "F9a-F", 0};
char * Codes[] = {
	"V[A0A][39X29W]",//[18A][19A][16A][17A]",//[18#3G][19#3G]"
	"A43#3mB",
	"B-Z[40B][41mC][42mC]",
	"Z[41a][42a]",
	"C44aC",
	"GA15-G",
	//"C11lC",
	// Цветок:
	"V29W",
	"W#8L26W", 
	"L[24[20m][21a][23a][22a]]-25L",
	// 4 Лепестка
	"X#4Y",
	"Y[34[30m][31a][33a][32a]]-35Y",
	0
};

CMat M0 = {WorkHeight * 0.5, 0, 0, WorkHeight * 0.5, WorkWidth * 0.5, WorkHeight * 0.5};
CMat fm[100] = {
	{0.85, 0.04, -0.04, 0.85, 0, 1.6},
	{0.20,-0.26, 0.23,  0.22, 0, 1.6, 5},
	{-0.15, 0.28, 0.26, 0.24, 0, 0.44, 5},

	{0.3, 0.1, -0.1, 0.3, 0.1, 0.4, 4},
	{0.3, -0.1, 0.1, 0.3, -0.2, 0.8, 4}
	//{0.4, 0.04, -0.04, 0.8, 0, 1.6},
};
CMat Stack[20], * SP = Stack;

bool mtUse = false;
double mtV[3];


void fMoveTo(const CMat & Pos)
{
	mtUse = true;
	mtV[0] = Pos.V[0];
	mtV[1] = Pos.V[1];
	mtV[2] = gr;
}

void fLineTo(const CMat & Pos)
{
	if(mtUse)
		MoveTo(mtV[0], mtV[1], mtV[2]);
	mtUse = false;
	mtV[0] = Pos.V[0];
	mtV[1] = Pos.V[1];
	mtV[2] = gr;
	LineTo(Pos.V[0], Pos.V[1], gr);
}

void fArcTo(const CMat & Pos)
{
	if(mtUse)
		MoveTo(mtV[0], mtV[1], mtV[2]);
	mtUse = false;
	mtV[0] -= Pos.V[0];
	mtV[1] -= Pos.V[1];
	if(double s = (mtV[0] * Pos.M[2] - mtV[1] * Pos.M[0]) / sqrt((Pos.M[0] * Pos.M[0] + Pos.M[2] * Pos.M[2]) * (mtV[0] * mtV[0] + mtV[1] * mtV[1])))
	{
		double a = asin(s) * (360 / M_PI);
		//if(a < 0) a += 360;
		ArcTo(Pos.V[0], Pos.V[1], gr, a);
	} else
		LineTo(Pos.V[0], Pos.V[1], gr);
	mtV[0] = Pos.V[0];
	mtV[1] = Pos.V[1];
	mtV[2] = gr;
}

void InitNorms(void)
{
	for(int x = 0; x < sizeof(fm) / sizeof(*fm); x++)
	{
		double k = fm[x].norm;
		fm[x].InitNorm();
		if(k) fm[x].norm *= k;
		//if(fm[x].norm >= 0.95) fm[x].norm = 0.95;
	}
}

void InitFractal(void)
{
	fm[0].Init(1.0, 180, 1);
	fm[0].V[0] = 0;
	fm[0].V[1] = 0;


	fm[18].Init(0.4, 30); // Правая сторона
	fm[18].V[0] = 0.7;
	fm[18].V[1] = 0.3;

	fm[19].Init(-0.4, -35, -1); // Левая сторона
	fm[19].V[0] = -0.7;
	fm[19].V[1] = 0.25;

	fm[16].Init(0.3, 15);
	fm[16].V[0] = 0.7;
	fm[16].V[1] = -0.5;

	fm[17].Init(-0.3, -15, -1);
	fm[17].V[0] = -0.7;
	fm[17].V[1] = -0.5;

	fm[15].Init(1.1, -20, 1); // Ниже
	fm[15].V[0] = -0.1;
	fm[15].V[1] = -0.7;

	double sa = -4;
	fm[11].Init(0.8, sa); // Стебель
	fm[11].V[0] = 0.8;
	fm[11].V[1] = fm[11].V[0] / (1 + cos(sa * M_PI / 180)) * sin(sa * M_PI / 180);

	fm[12].Init(0.5, 20); // Ветвление вверх
	fm[12].V[0] = 0.2;
	fm[12].V[1] = -0.001;
	fm[12].norm *= 1.2;

	fm[13].Init(0.5, -30, -1); // Ветвление вниз
	fm[13].V[0] = 0.5;
	fm[13].V[1] = -0.02;
	fm[13].norm *= 1.2;

// Змейка
	fm[40].Init(0.7, 0);
	fm[40].V[0] = 1.0;
	fm[40].V[1] = 0;

	fm[41].Init(0.4, -45, -1);
	fm[41].V[0] = 0.5;
	fm[41].V[1] = 0;

	fm[42].Init(0.4, 45);
	fm[42].V[0] = 1.0;
	fm[42].V[1] = 0;

	fm[43].Init(1.1, 0);
	fm[43].V[0] = 0.7;
	fm[43].V[1] = 0;

	fm[44].Init(0.7, 30);
	fm[44].V[0] = 0.5;
	fm[44].V[1] = 0.15;
	fm[44].norm = 0.95;

// 4 больших лепестка
	fm[30].Init(0.5, 0); // Левый край лепестка
	fm[30].V[0] = -1.2;
	fm[30].V[1] = -0.43;//0.05;
	fm[31].Init(0.5, 45);
	fm[31].V[0] = -0.15;//0.0;
	fm[31].V[1] = 0.8;

	fm[33].Init(0.5, -40);
	fm[33].V[0] = 0.15;
	fm[33].V[1] = 0.8;

	fm[32].Init(0.5, 140); // Правый край лепестка
	fm[32].V[0] = 1.3;
	fm[32].V[1] = -0.53;

	fm[35].Init(1.0, 90); // Матрица поворота лепестка
	fm[35].V[0] = 0.0;
	fm[35].V[1] = 0.0;

	fm[34].Init(0.2, 0, 3);
	fm[34].V[0] = 0.0;
	fm[34].V[1] = 0.82;

	fm[39].Init(0.85, 35);
	fm[39].V[0] = 0.0;
	fm[39].V[1] = 0.0;

// Цветок //{"A#12L26A", "L[24[20m][21a][22a]]-25L"}!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	double ain = 20;
	double aout = 20;
	double gold = (sqrt(5.0) + 1.0) / 2.0;

	fm[20].Init(0.5, 0); // Левый край лепестка
	fm[20].V[0] = -1.0;
	fm[20].V[1] = -0.11;//0.05;
	fm[21].Init(0.5, 30);
	fm[21].V[0] = -0.2;//0.0;
	fm[21].V[1] = 1.0;

	fm[23].Init(0.5, -35);
	fm[23].V[0] = 0.2;
	fm[23].V[1] = 1.0;

	fm[22].Init(0.5, 150); // Правый край лепестка
	fm[22].V[0] = 0.95;
	fm[22].V[1] = 0.07;


	fm[24].Init(0.2, 0, 3);
	fm[24].V[0] = 0.0;
	fm[24].V[1] = 0.82;

	fm[25].Init(1.0, -45); // Матрица поворота лепестка
	fm[25].V[0] = 0.0;
	fm[25].V[1] = 0.0;

	fm[26].Init(0.75, 25); // Матрица уменьшения лепестков
	fm[26].V[0] = 0.0;
	fm[26].V[1] = 0.0;

	fm[29].Init(0.65, 4);
	fm[29].V[0] = 0.0;
	fm[29].V[1] = 0.0;
// Цветок /////////////////////////////////////////////////////////
}

bool Fractal(const CMat &M, char Code)
{
	if(!M.TTL) return false;
	bool rec = (M.norm > 5.0);
	char * * P;
	for(P = Codes; *P; P++)
		if(**P == Code) break;
	_ASSERTE(*P);
	if(!*P) return false;
	CMat m = M;
	CMat * BP = SP;
	for(const char * p = *P + 1; *p; p++)
	{
		char c = *p;
		if(c == 'm') {fMoveTo(m); continue;};
		if(c == 'l') {fLineTo(m); continue;};
		if(c == 'a') {fArcTo(m); continue;};
		if(c >= '0' && c <= '9')
		{
			int n = 0;
			for(; c >= '0' && c <= '9'; c = *(++p))
				n = n * 10 + c - '0';
			p--;
			m *= fm[n]; continue;
		};
		if(c == '#')
		{
			int n = 0;
			for(p++; *p >= '0' && *p <= '9'; p++)
				n = n * 10 + *p - '0';
			p--;
			m.TTL = n;
			continue;
		}
		if(c == '-') { m.TTL--; continue;};
		if(c == '[')
		{
			*(SP++) = m;
			_ASSERTE(SP < Stack + sizeof(Stack) / sizeof(*Stack));
			continue;
		}
		if(c == ']')
		{
			_ASSERTE(SP > Stack);
			m = *(--SP);
			continue;
		}
		/*if(c == '[') 
		{
			int x;
			for(x = 1, p++; *p && x; p++)
				if(*p == '[') x++;
				else if(*p == ']') x--;
			_ASSERTE(*p && !x);
			p--;
			continue;
		}
		_ASSERTE(c != ']');*/
		if(c == ' ') continue;
		if(rec)
		if(!Fractal(m, c))
		{
			SP = BP;
			return true;
		}
	}
	/*m = M;
	for(const char * p = *P + 1; *p; p++)
	{
		char c = *p;
		if(c == 'm') {if(SP > Stack) fMoveTo(m); continue;};
		if(c == 'l') {if(SP > Stack) fLineTo(m); continue;};
		if(c >= '0' && c <= '9') {m *= fm[c - '0']; continue;};
		if(c == '[')
		{
			*(SP++) = m;
			_ASSERTE(SP < Stack + sizeof(Stack) / sizeof(*Stack));
			continue;
		}
		if(c == ']')
		{
			_ASSERTE(SP > Stack);
			m = *(--SP);
			continue;
		}
		if(SP > Stack)Fractal(m, c);
	}*/


	return true;
}

double CalcAngle(double x, double y, double x1, double y1, double x2, double y2)
{
	x1 -= x; x2 -= x; y1 -= y; y2 -= y;
	return 180 / M_PI * acos((x1 * x2 + y1 * y2) / sqrt((x1 * x1 + y1 * y1) * (x2 * x2 + y2 * y2)));
}

void LadaOuter(double a, double x, double y, T2D * os, int Count, int TC)
{
	double zz = 1.7;
	double d;
	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		double o = os[c].X;
		o /= a;
		double r2 = 5.5 + o;
		double o1 = o * sqrt(2.0);

		double cx1 = 6, cy1 = 6;
		SolveArc(cx1, cy1, r2, 0, 1 - o1, 1, 2 - o1);
		double an2 = CalcAngle(6, 6, cx1, cy1, cy1, cx1);

		double cx = 6, cy = 6;
		SolveArc(cx, cy, r2, 0, 3 + o1, 1, 4 + o1);
		double an1 = CalcAngle(6, 6, cx, cy, 0, 6) * 2;
		if(c)
			LineTo(cx1 * a + x, cy1 * a + y, d);
		else
			MoveTo(cx1 * a + x, cy1 * a + y, d);
		LineTo((0 - o) * a + x, (1 - o1 - o) * a + y, d);
		if(c > TC)
		{
			for(int cc = c; cc >= TC; cc--)
				LineTo(0 * a + x - os[cc].X, 2 * a + y - zz, os[cc].Y);
			for(int cc = TC; cc <= c; cc++)
				LineTo(0 * a + x - os[cc].X, 2 * a + y + zz, os[cc].Y);
		}
		LineTo((0 - o) * a + x, (3 + o1 - o) * a + y, d);
		LineTo(cx * a + x, cy * a + y, d);
		ArcTo(cx * a + x, (12 - cy) * a + y, d, an1);
		LineTo((0 - o) * a + x, (9 - o1 + o) * a + y, d);
		LineTo((0 - o) * a + x, (11 + o1 + o) * a + y, d);
		LineTo(cx1 * a + x, (12 - cy1) * a + y, d);
		ArcTo((cy1) * a + x, (12 - cx1) * a + y, d, an2);
		LineTo((1 - o1 - o) * a + x, (12 + o) * a + y, d);
		if(c > TC)
		{
			for(int cc = c; cc >= TC; cc--)
				LineTo(2 * a + x - zz, 12 * a + y + os[cc].X, os[cc].Y);
			for(int cc = TC; cc <= c; cc++)
				LineTo(2 * a + x + zz, 12 * a + y + os[cc].X, os[cc].Y);
		}
		LineTo((3 + o1 - o) * a + x, (12 + o) * a + y, d);
		LineTo(cy * a + x, (12 - cx) * a + y, d);
		ArcTo((12 - cy) * a + x, (12 - cx) * a + y, d, an1);
		LineTo((9 - o1 + o) * a + x, (12 + o) * a + y, d);
		LineTo((11 + o1 + o) * a + x, (12 + o) * a + y, d);
		LineTo((12 - cy1) * a + x, (12 - cx1) * a + y, d);
		ArcTo((12 - cx1) * a + x, (12 - cy1) * a + y, d, an2);
		LineTo((12 + o) * a + x, (11 + o1 + o) * a + y, d);
		if(c > TC)
		{
			for(int cc = c; cc >= TC; cc--)
				LineTo(12 * a + x + os[cc].X,10 * a + y + zz, os[cc].Y);
			for(int cc = TC; cc <= c; cc++)
				LineTo(12 * a + x + os[cc].X, 10 * a + y - zz, os[cc].Y);
		}
		LineTo((12 + o) * a + x, (9 - o1 + o) * a + y, d);
		LineTo((12 - cx) * a + x, (12 - cy) * a + y, d);
		ArcTo((12 - cx) * a + x, (cy) * a + y, d, an1);
		LineTo((12 + o) * a + x, (3 + o1 - o) * a + y, d);
		LineTo((12 + o) * a + x, (1 - o1 - o) * a + y, d);
		LineTo((12 - cx1) * a + x, (cy1) * a + y, d);
		ArcTo((12 - cy1) * a + x, (cx1) * a + y, d, an2);
		LineTo((11 + o + o1) * a + x, (0 - o) * a + y, d);
		if(c > TC)
		{
			for(int cc = c; cc >= TC; cc--)
				LineTo(10 * a + x + zz, 0 * a + y - os[cc].X, os[cc].Y);
			for(int cc = TC; cc <= c; cc++)
				LineTo(10 * a + x - zz, 0 * a + y - os[cc].X, os[cc].Y);
		}
		LineTo((9 + o - o1) * a + x, (0 - o) * a + y, d);
		LineTo((12 - cy) * a + x, cx * a + y, d);
		ArcTo(cy * a + x, cx * a + y, d, an1);
		LineTo((3 - o + o1) * a + x, (0 - o) * a + y, d);
		LineTo((1 - o - o1) * a + x, (0 - o) * a + y, d);
		LineTo(cy1 * a + x, cx1 * a + y, d);
		ArcTo(cx1 * a + x, cy1 * a + y, d, an2);
	}
	for(int c = Count - 1; c >= 0; c--)
	{
		d = os[c].Y;
		double o = os[c].X;
		o /= a;
		double r2 = 5.5 + o;
		double o1 = o * sqrt(2.0);
		double cx1 = 6, cy1 = 6;
		SolveArc(cx1, cy1, r2, 0, 1 - o1, 1, 2 - o1);
		LineTo(cx1 * a + x, cy1 * a + y, d);


	}
}

void LadaInner(double a, double x, double y,  T2D * os, int Count)
{
	struct TData
	{
		double cx, cy;
		double cx1, cy1;
		double cx2, cy2;
		double cx3, cy3;
		double an1, an2;
		double o;
	} * D;
	D = (TData *) malloc(sizeof(TData) * Count);
	for(int c = 0; c < Count; c++)
	{
		D[c].o = os[c].X / a;

		double r1 = 4 - D[c].o;
		D[c].o *= sqrt(2.0);
		D[c].cx = 6; D[c].cy = 6;
		SolveArc(D[c].cx, D[c].cy, r1, 8, 7 + D[c].o, 9, 8 + D[c].o);

		D[c].cx1 = 6; D[c].cy1 = 6;
		SolveArc(D[c].cx1, D[c].cy1, r1, 8, 9 - D[c].o, 9, 10 - D[c].o);
		D[c].an1 = CalcAngle(6, 6, D[c].cx, D[c].cy, D[c].cx1, D[c].cy1);

		D[c].cx2 = 6; D[c].cy2 = 6;
		SolveArc(D[c].cx2, D[c].cy2, r1, 9, 6 - D[c].o, 10, 7 - D[c].o);
		D[c].cx3 = 6; D[c].cy3 = 6;
		SolveArc(D[c].cx3, D[c].cy3, r1, 9, 6 + D[c].o, 10, 5 + D[c].o);
		D[c].an2 = CalcAngle(6, 6, D[c].cx2, D[c].cy2, D[c].cx3, D[c].cy3);
	}
	double d;
	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo(D[c].cx * a + x, D[c].cy * a + y, d);
		else LineTo(D[c].cx * a + x, D[c].cy * a + y, d);
		LineTo(8 * a + x, (7 + D[c].o) * a + y, d);
		LineTo((7 + D[c].o) * a + x, 8 * a + y, d);
		LineTo(D[c].cx1 * a + x, D[c].cy1 * a + y, d);
		ArcTo(D[c].cx * a + x, D[c].cy * a + y, d, D[c].an1);
	}
	LineTo(8.5 * a + x, 8.5 * a + y, d);


	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo((9 + D[c].o) * a + x, 6 * a + y, d);
		else LineTo((9 + D[c].o) * a + x, 6 * a + y, d);
		LineTo(D[c].cx2 * a + x, D[c].cy2 * a + y, d);
		ArcTo(D[c].cx3 * a + x, D[c].cy3 * a + y, d, D[c].an2);
		LineTo((9 + D[c].o) * a + x, 6 * a + y, d);
	}
	LineTo(9.5 * a + x, 6 * a + y, d);

	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo(8 * a + x, (5 - D[c].o) * a + y, d);
		else LineTo(8 * a + x, (5 - D[c].o) * a + y, d);
		LineTo(D[c].cx * a + x, (12 - D[c].cy) * a + y, d);
		ArcTo(D[c].cx1 * a + x, (12 - D[c].cy1) * a + y, d, D[c].an1);
		LineTo((7 + D[c].o) * a + x, 4 * a + y, d);
		LineTo(8 * a + x, (5 - D[c].o) * a + y, d);
	}
	LineTo(8.0 * a + x, 4.5 * a + y, d);


	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo(6 * a + x, (3 - D[c].o) * a + y, d);
		else LineTo(6 * a + x, (3 - D[c].o) * a + y, d);
		LineTo(D[c].cy2 * a + x, (12 - D[c].cx2) * a + y, d);
		ArcTo(D[c].cy3 * a + x, (12 - D[c].cx3) * a + y, d, D[c].an2);
		LineTo(6 * a + x, (3 - D[c].o) * a + y, d);
	}
	LineTo(6 * a + x, 2.5 * a + y, d);

	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo((5 - D[c].o) * a + x, 4 * a + y, d);
		else LineTo((5 - D[c].o) * a + x, 4 * a + y, d);
		LineTo((12 - D[c].cy) * a + x, (12 - D[c].cx) * a + y, d);
		ArcTo((12 - D[c].cx) * a + x, (12 - D[c].cy) * a + y, d, D[c].an1);
		LineTo(4 * a + x, (5 - D[c].o) * a + y, d);
		LineTo((5 - D[c].o) * a + x, 4 * a + y, d);
	}
	LineTo(4.5 * a + x, 4.0 * a + y, d);

	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo((3 - D[c].o) * a + x, 6 * a + y, d);
		else LineTo((3 - D[c].o) * a + x, 6 * a + y, d);
		LineTo((12 - D[c].cx2) * a + x, D[c].cy3 * a + y, d);
		ArcTo((12 - D[c].cx2) * a + x, D[c].cy2 * a + y, d, D[c].an2);
		LineTo((3 - D[c].o) * a + x, 6 * a + y, d);
	}
	LineTo(2.5 * a + x, 6.0 * a + y, d);

	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo((5 - D[c].o) * a + x, 8 * a + y, d);
		else LineTo((5 - D[c].o) * a + x, 8 * a + y, d);
		LineTo(4 * a + x, (7 + D[c].o) * a + y, d);
		LineTo((12 - D[c].cy1) * a + x, D[c].cx1 * a + y, d);
		ArcTo((12 - D[c].cy) * a + x, D[c].cx * a + y, d, D[c].an1);
		LineTo((5 - D[c].o) * a + x, 8 * a + y, d);
	}
	LineTo(4.5 * a + x, 8.0 * a + y, d);
	
	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo(6 * a + x, (9 + D[c].o) * a + y, d);
		else LineTo(6 * a + x, (9 + D[c].o) * a + y, d);
		LineTo(D[c].cy3 * a + x, D[c].cx3 * a + y, d);
		ArcTo(D[c].cy2 * a + x, D[c].cx2 * a + y, d, D[c].an2);
		LineTo(6 * a + x, (9 + D[c].o) * a + y, d);
	}
	LineTo(6.0 * a + x, 9.5 * a + y, d);

	for(int c = 0; c < Count; c++)
	{
		d = os[c].Y;
		if(!c) MoveTo(6 * a + x, (7 - D[c].o) * a + y, d);
		else LineTo(6 * a + x, (7 - D[c].o) * a + y, d);
		LineTo((7 - D[c].o) * a + x, 6 * a + y, d);
		LineTo(6 * a + x, (5 + D[c].o) * a + y, d);
		LineTo((5 + D[c].o) * a + x, 6 * a + y, d);
		LineTo(6 * a + x, (7 - D[c].o) * a + y, d);
	}
	LineTo(6.0 * a + x, 6.0 * a + y, d);
	free(D);
}

void LadaGrave(double a, double x, double y)
{
	double r1 = 4, r2 = 5.5;
	//1:
	MoveTo(5 * a + x, 4 * a + y, gr);
	LineTo(1 * a + x, y, gr);
	LineTo(3 * a + x, y, gr);
	LineTo(6 * a + x, 3 * a + y, gr);
	LineTo(4 * a + x, 5 * a + y, gr);
	LineTo(6 * a + x, 7 * a + y, gr);
	LineTo(5 * a + x, 8 * a + y, gr);
	LineTo(3 * a + x, 6 * a + y, gr);
	LineTo(0 * a + x, 9 * a + y, gr);
	LineTo(0 * a + x, 11 * a + y, gr);
	LineTo(4 * a + x, 7 * a + y, gr);

	//2:
	MoveTo(7 * a + x, 8 * a + y, gr);
	LineTo(11 * a + x, 12 * a + y, gr);
	LineTo(9 * a + x, 12 * a + y, gr);
	LineTo(6 * a + x, 9 * a + y, gr);
	LineTo(8 * a + x, 7 * a + y, gr);
	LineTo(6 * a + x, 5 * a + y, gr);
	LineTo(7 * a + x, 4 * a + y, gr);
	LineTo(9 * a + x, 6 * a + y, gr);
	LineTo(12 * a + x, 3 * a + y, gr);
	LineTo(12 * a + x, 1 * a + y, gr);
	LineTo(8 * a + x, 5 * a + y, gr);

	//3:
	double cx = 6, cy = 6;
	SolveArc(cx, cy, r1, 8, 5, 12, 1);
	MoveTo(cx * a + x, cy * a + y, gr);
	double cx1 = 6, cy1 = 6;
	SolveArc(cx1, cy1, r1, 3, 0, 6, 3);
	double an1 = CalcAngle(6, 6, cx, cy, cx1, cy1);
	ArcTo(cx1 * a + x, cy1 * a + y, gr, an1);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 3, 0, 6, 3);
	MoveTo(cx * a + x, cy * a + y, gr);
	cx1 = 6; cy1 = 6;
	SolveArc(cx1, cy1, r2, 8, 5, 12, 1);
	double an2 = CalcAngle(6, 6, cx, cy, cx1, cy1);
	ArcTo(cx1 * a + x, cy1 * a + y, gr, -an2);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 11, 0, 10, 1);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(11 * a + x, 0 * a + y, gr);
	LineTo(9 * a + x, 0 * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 9, 0, 8, 1);
	LineTo(cx * a + x, cy * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 9, 0, 8, 1);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(6 * a + x, 3 * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 10, 1, 9, 2);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(5 * a + x, 6 * a + y, gr);
	
	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 0, 1, 1, 2);
	LineTo(cx * a + x, cy * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 0, 1, 1, 2);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(0 * a + x, 1 * a + y, gr);
	LineTo(0 * a + x, 3 * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 0, 3, 1, 4);
	LineTo(cx * a + x, cy * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 0, 3, 1, 4);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(3 * a + x, 6 * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 3, 6, 2, 7);
	MoveTo(cx * a + x, cy * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 1, 0, 2, 1);
	ArcTo(cx * a + x, cy * a + y, gr, -an1);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 1, 0, 2, 1);
	MoveTo(cx * a + x, cy * a + y, gr);
	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 3, 6, 2, 7);
	ArcTo(cx * a + x, cy * a + y, gr, an2);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 3, 8, 4, 7);
	MoveTo(cx * a + x, cy * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 6, 9, 7, 10);
	ArcTo(cx * a + x, cy * a + y, gr, an1);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 6, 9, 5, 10);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(6 * a + x, 9 * a + y, gr);
	MoveTo(7 * a + x, 6 * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 7, 6, 6, 7);
	LineTo(cx * a + x, cy * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 6, 9, 5, 10);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(3 * a + x, 12 * a + y, gr);
	LineTo(1 * a + x, 12 * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 7, 6, 6, 7);
	LineTo(cx * a + x, cy * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 0, 11, 1, 10);
	MoveTo(cx * a + x, cy * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 6, 9, 7, 10);
	ArcTo(cx * a + x, cy * a + y, gr, an2);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 7, 8, 8, 9);
	MoveTo(cx * a + x, cy * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 11, 4, 12, 3);
	ArcTo(cx * a + x, cy * a + y, gr, an2);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 11, 4, 12, 3);
	MoveTo(cx * a + x, cy * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 7, 8, 8, 9);
	ArcTo(cx * a + x, cy * a + y, gr, -an1);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 7, 6, 8, 7);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(8 * a + x, 7 * a + y, gr);
	MoveTo(9 * a + x, 6 * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r1, 9, 6, 10, 7);
	LineTo(cx * a + x, cy * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 9, 6, 10, 7);
	MoveTo(cx * a + x, cy * a + y, gr);
	LineTo(12 * a + x, 9 * a + y, gr);
	LineTo(12 * a + x, 11 * a + y, gr);

	cx = 6; cy = 6;
	SolveArc(cx, cy, r2, 12, 11, 11, 10);
	LineTo(cx * a + x, cy * a + y, gr);
}

void Lada(void)
{
	double VStep = 0.38; // мм
	double AA = 6.0;
	LadaGrave(AA, 1.5, 1.5);
	double vs = VStep / (BotZ - TopZ);
	double mx = 1.0 + 2.3 / (BotZ - TopZ);
	int Count = (int)ceil((mx - gr) / vs) - 1;
	T2D * pts = (T2D *) malloc(sizeof(T2D) * Count);
	for(int x = 0; x < Count; x++)
	{
		double X = (double)(x + 1) / (Count - 1);
		pts[x].X = -2.0 * X * X + 3.0 * X;//(1 - exp((double)-(x))) * 1.1 - x * 0.01;
		pts[x].Y = double(x + 1) * vs + gr;
	}

	LadaInner(AA, 1.5, 1.5, pts, Count);
	LadaOuter(AA, 1.5, 1.5, pts, Count, int(4.0 / VStep));
	free(pts);
}

void Heart(double a, double x, double y, double d)
{
	MoveTo(a * 0.5 + x, y, d);
	LineTo(a * 1.0 + x, a * 0.5 + y, d);
	ArcTo(a * 0.5 + x, a * 1.0 + y, d, -180);
	ArcTo(a * 0.0 + x, a * 0.5 + y, d, -180);
	LineTo(a * 0.5 + x, a * 0.0 + y, d);
}

void Flower4(double a, double x, double y, double d)
{
	MoveTo(a * 0.5 + x, y, d);
	ArcTo(a * 1.0 + x, a * 0.5 + y, d, -180);
	ArcTo(a * 0.5 + x, a * 1.0 + y, d, -180);
	ArcTo(a * 0.0 + x, a * 0.5 + y, d, -180);
	ArcTo(a * 0.5 + x, a * 0.0 + y, d, -180);
}

void FlowerN(double a, double x, double y, double d)
{
	int N = 8;
	//MoveTo(a * 0.5 + x, y, d);
	for(int n = 0; n <= N; n++)
	{
		double an = ((double)(n - 0.5) / (double)N) * 2.0 * M_PI;
		if(!n)
			MoveTo(a * (sin(an) * 0.5 + 0.5) + x, a * (-cos(an) * 0.5 + 0.5) + y, d);
		else
			ArcTo(a * (sin(an) * 0.5 + 0.5) + x, a * (-cos(an) * 0.5 + 0.5) + y, d, (n & 1) ? -30 : -200);
		

	}
	//ArcTo(a * 0.5 + x, a * 1.0 + y, d, -180);
	//ArcTo(a * 0.0 + x, a * 0.5 + y, d, -180);
	//ArcTo(a * 0.5 + x, a * 0.0 + y, d, -180);
}


int _tmain(int argc, _TCHAR* argv[])
{
	printf("G53 G0 Z0 G0 X0 Y0\n");
	printf("G0 X%.3f Y%.3f G0 Z%.3f\n", DX, DY, SafeZ);
/*MoveTo(10, 10, gr);
	ArcTo(20, 20, gr, 90);
	double t = 10 / sqrt(2.0);
	LineTo(35 - t, 20, gr);
	ArcTo(35, 10 - t, gr, 135);
	ArcTo(35 + t, 20, gr, 135);
	LineTo(50, 20, gr);
	ArcTo(60, 10, gr, 90);
	ArcTo(50, 20, gr, 90);
	LineTo(50, 25, gr);
	ArcTo(60, 35, gr, 90);
	ArcTo(50, 25, gr, 90);
	LineTo(35 + t, 25, gr);
	ArcTo(35, 35 + t, gr, 135);
	ArcTo(35 - t, 25, gr, 135);
	LineTo(20, 25, gr);
	ArcTo(10, 35, gr, 90);
	ArcTo(20, 25, gr, 90);
	LineTo(20, 20, gr);
	ArcTo(10, 10, gr, 90);*/

	/*MoveTo(80, 10, gr);
	ArcTo(75, 5, gr, 90);
	LineTo(15, 5, gr);
	ArcTo(15, 15, gr, 180);
	ArcTo(5, 15, gr, 180);
	LineTo(5, 40, gr);
	ArcTo(10, 35, gr, -45);
	LineTo(10, 20, gr);
	ArcTo(20, 10, gr, -90);*/

	/*// Фрактал
	InitNorms();
	InitFractal();
	M0.InitNorm();
	M0.TTL = 10;
	Fractal(M0, 'V');*/
	/*for(double d = 0.0; d <= 1.0; d += 0.2) 
	{
		//Heart(170, 20, 0, d);
		//Flower4(150, 30, 20, d);
		FlowerN(160, 20, 15, d);
	}*/

	Lada();

	printf("G0 Z0 G0 X0 Y0\n");
	ShowLines();
	return 0;
}

