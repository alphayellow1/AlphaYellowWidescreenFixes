#include <conio.h>
#include <direct.h>
#include <fstream>
#include <iostream>
#include <locale.h>
#include <stdint.h>
#include <windows.h>

using namespace std;

int main()
{
	int language;
	cout << "Beyond Normandy: Assignment Berlin widescreen patch by AuToMaNiAk005, 2024\n\n----------------\n\n";
	cout << "1: English\n";
	cout << "2: Polski\n";
	cout << "\n----------------\n\n";
	cin >> language;
	cout << "\n----------------\n\n";
	uint8_t check1;
	uint8_t check2;
	bool isFileCorrect;
	int newWidth;
	int newHeight;
	uint8_t newWidthSmall;
	uint8_t newWidthBig;
	uint8_t newHeightSmall;
	uint8_t newHeightBig;
	float aspectRatio;
	float hudWidth;
	float hudMargin;
	float fov;
	float fovMultiplier;
	float flt;
	double dbl;
	fstream binaryFile("AssignmentBerlin-V1.05.exe", ios::in | ios::out | ios::binary);
	binaryFile.seekg(0x000568B2);
	binaryFile >> check1;
	binaryFile.seekg(0x000568B3);
	binaryFile >> check2;
	if (check1 == 0xF8 && check2 == 0xD0)
	{
		isFileCorrect = true;
	}
	else
	{
		isFileCorrect = false;
	}
	if (isFileCorrect == true)
	{
		if (language == 1)
		{
			cout << "Type your resolution width: ";
			cin >> newWidth;
			if (newWidth == 0)
			{
				cout << "Are you kidding me?";
				getch();
				return 0;
			}
			if (newWidth < 0)
			{
				cout << "Did you really type a negative number?";
				getch();
				return 0;
			}
			if (newWidth > 65535)
			{
				cout << "Back in 2024, we weren't using screens with such high resolutions and I was too lazy to add support for such high values. Please type a lower resolution (less than 65536 of width and height) which matches your target aspect ratio and put your values with a hex editor at 0009345A (width) and 00093462 (height) offsets.";
				getch();
				return 0;
			}
			cout << "Type your resolution height: ";
			cin >> newHeight;
			if (newHeight == 0)
			{
				cout << "No.";
				getch();
				return 0;
			}
			if (newHeight < 0)
			{
				cout << "Why don't you just flip your screen upside down?";
				getch();
				return 0;
			}
			if (newHeight > 65535)
			{
				cout << "Back in 2024, we weren't using screens with such high resolutions and I was too lazy to add support for such high values. Please type a lower resolution (less than 65536 of width and height) which matches your target aspect ratio and put your values with a hex editor at 0009345A (width) and 00093462 (height) offsets.";
				getch();
				return 0;
			}
		}
		else if (language == 2)
		{
			setlocale(LC_CTYPE, "Polish");
			cout << "Wpisz szeroko�� rozdzielczo�ci: ";
			cin >> newWidth;
			if (newWidth == 0)
			{
				cout << "Jaja sobie robisz?";
				getch();
				return 0;
			}
			if (newWidth < 0)
			{
				cout << "Naprawd� wpisa�e� liczb� ujemn�?";
				getch();
				return 0;
			}
			if (newWidth > 65535)
			{
				cout << "W 2024 roku, nie korzystali�my z ekran�w o tak wysokiej rozdzielczo�ci, a ja by�em zbyt leniwy, by doda� wsparcie dla tak wysokich warto�ci. Prosz� poda� ni�sz� rozdzielczo�� (mniejsza od 65536 w szeroko�ci i wysoko�ci) kt�ra jest zgodna w proporcjami Twojego ekranu i umie�ci� swoje warto�ci za pomoc� edytora szesnastkowego w offsetach 0009345A (szeroko��) i 00093462 (wysoko��).";
				getch();
				return 0;
			}
			cout << "Wpisz wysoko�� rozdzielczo�ci: ";
			cin >> newHeight;
			if (newHeight == 0)
			{
				cout << "Nie.";
				getch();
				return 0;
			}
			if (newHeight < 0)
			{
				cout << "Dlaczego po prostu nie odwr�cisz swojego ekranu do g�ry nogami?";
				getch();
				return 0;
			}
			if (newHeight > 65535)
			{
				cout << "W 2024 roku, nie korzystali�my z ekran�w o tak wysokiej rozdzielczo�ci, a ja by�em zbyt leniwy, by doda� wsparcie dla tak wysokich warto�ci. Prosz� poda� ni�sz� rozdzielczo�� (mniejsza od 65536 w szeroko�ci i wysoko�ci) kt�ra jest zgodna w proporcjami Twojego ekranu i umie�ci� swoje warto�ci za pomoc� edytora szesnastkowego w offsetach 0009345A (szeroko��) i 00093462 (wysoko��).";
				getch();
				return 0;
			}
		}
		else
		{
			return 0;
		}
		cout << "\n----------------\n\n";
		newWidthSmall = newWidth % 256;
		newWidthBig = (newWidth - newWidthSmall) / 256;
		newHeightSmall = newHeight % 256;
		newHeightBig = (newHeight - newHeightSmall) / 256;
		binaryFile.seekp(0x0009345A);
		binaryFile << char(newWidthSmall);
		binaryFile.seekp(0x0009345B);
		binaryFile << char(newWidthBig);
		binaryFile.seekp(0x00093462);
		binaryFile << char(newHeightSmall);
		binaryFile.seekp(0x00093463);
		binaryFile << char(newHeightBig);
		binaryFile.seekp(0x00013D70);
		binaryFile << char(newWidthSmall);
		binaryFile.seekp(0x00013D71);
		binaryFile << char(newWidthBig);
		binaryFile.seekp(0x00013D78);
		binaryFile << char(newHeightSmall);
		binaryFile.seekp(0x00013D79);
		binaryFile << char(newHeightBig);
		aspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);
		hudWidth = 600 * aspectRatio;
		if (language == 1)
		{
			cout << "Type your FOV multiplier\n(use dot as a separator; 1 = default; at high values, the character's hands may appear unfinished):\n";
			cin >> fovMultiplier;
			cout << "Type your HUD margin (0 = default, " << hudWidth / 2 - 400 << " = centered to 4:3):\n";
			cin >> hudMargin;
		}
		else if (language == 2)
		{
			cout << "Wpisz mno�nik pola widzenia\n(u�yj kropki jako separator, 1 = domy�lny, przy wysokich warto�ciach r�ce postaci mog� wygl�da� na niekompletne):\n";
			cin >> fovMultiplier;
			cout << "Wpisz margines dla interfejsu (0 = domy�lny, " << hudWidth / 2 - 400 << " = wy�rodkowany do 4:3):\n";
			cin >> hudMargin;
		}
		cout << "\n----------------\n\n";
		// Compass needle width
		flt = 0.0333333 / aspectRatio;
		const unsigned char *pf = reinterpret_cast<const unsigned char *>(&flt);
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x0019D0F8 + i);
			binaryFile << char(pf[i]);
		}
		// FOV
		flt = 0.75 * aspectRatio * fovMultiplier;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x0019D0FC + i);
			binaryFile << char(pf[i]);
		}
		// Loading bar X
		flt = 222 + (hudWidth - 800) / 2;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A707C + i);
			binaryFile << char(pf[i]);
		}
		// Red cross and menu text width
		flt = 0.001666 / aspectRatio;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A7084 + i);
			binaryFile << char(pf[i]);
		}
		// HUD left margin
		flt = 0.0333333 / aspectRatio + hudMargin / hudWidth;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A7128 + i);
			binaryFile << char(pf[i]);
		}
		// Compass needle X
		flt = (hudWidth - 79 - hudMargin) / hudWidth;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A8200 + i);
			binaryFile << char(pf[i]);
		}
		// Objective direction width
		dbl = 0.001666 / aspectRatio;
		pf = reinterpret_cast<const unsigned char *>(&dbl);
		for (size_t i = 0; i != sizeof(double); ++i)
		{
			binaryFile.seekp(0x001A8220 + i);
			binaryFile << char(pf[i]);
		}
		// Compass X
		flt = (hudWidth - 118 - hudMargin) / hudWidth;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A822C + i);
			binaryFile << char(pf[i]);
		}
		// Compass width
		flt = 0.163333 / aspectRatio;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A8234 + i);
			binaryFile << char(pf[i]);
		}
		// Weapons list width
		flt = 0.17 / aspectRatio;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A8244 + i);
			binaryFile << char(pf[i]);
		}
		// HUD text width
		dbl = 0.0006666666666 / aspectRatio;
		pf = reinterpret_cast<const unsigned char *>(&dbl);
		for (size_t i = 0; i != sizeof(double); ++i)
		{
			binaryFile.seekp(0x001A8328 + i);
			binaryFile << char(pf[i]);
		}
		// Ammo X
		flt = (hudWidth - 74 - hudMargin) / hudWidth;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A8354 + i);
			binaryFile << char(pf[i]);
		}
		// Ammo Width
		flt = 0.116666 / aspectRatio;
		pf = reinterpret_cast<const unsigned char *>(&flt);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001A835C + i);
			binaryFile << char(pf[i]);
		}
		if (language == 1)
		{
			cout << "AssignmentBerlin-V1.05.exe file has been patched.\nYou can set your new resolution in the game options.";
		}
		else if (language == 2)
		{
			cout << "Plik AssignmentBerlin-V1.05.exe zosta� zaktualizowany.\nMo�esz ustawi� swoj� now� rozdzielczo�� w opcjach gry.";
		}
		getch();
	}
	else
	{
		if (language == 1)
		{
			cout << "Unsupported .exe file.\nMake sure you've put the included AssignmentBerlin-V1.05.exe file in the game directory.";
		}
		else if (language == 2)
		{
			setlocale(LC_CTYPE, "Polish");
			cout << "Nieobs�ugiwany plik .exe.\nUpewnij si�, �e umie�ci�e� za��czony plik AssignmentBerlin-V1.05.exe w folderze z gr�.";
		}
		getch();
	}
	return 0;
}
