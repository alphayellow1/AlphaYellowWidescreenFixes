#include <conio.h>
#include <fstream>
#include <iostream>
#include <stdint.h>

using namespace std;

int main()
{
	int oldWidth;
	int oldHeight;
	int newWidth;
	int newHeight;
	float fov;
	uint8_t check1;
	uint8_t check2;
	uint8_t oldWidthSmall;
	uint8_t oldWidthBig;
	uint8_t oldHeightSmall;
	uint8_t oldHeightBig;
	uint8_t newWidthSmall;
	uint8_t newWidthBig;
	uint8_t newHeightSmall;
	uint8_t newHeightBig;
	fstream binaryFile("SecretAgent.exe", ios::in | ios::out | ios::binary);
	binaryFile.seekg(0x001448A0);
	binaryFile >> check1;
	binaryFile.seekg(0x001448A1);
	binaryFile >> check2;
	if (check1 == 199 && check2 == 64)
	{
		binaryFile.seekg(0x001448A3);
		binaryFile >> oldWidthSmall;
		binaryFile.seekg(0x001448A4);
		binaryFile >> oldWidthBig;
		binaryFile.seekg(0x001448AE);
		binaryFile >> oldHeightSmall;
		binaryFile.seekg(0x001448AF);
		binaryFile >> oldHeightBig;
		oldWidth = oldWidthSmall + oldWidthBig * 256;
		oldHeight = oldHeightSmall + oldHeightBig * 256;
		cout << "Your current resolution: " << int(oldWidth) << "x" << int(oldHeight) << "\n";
		cout << "Type your new resolution width: ";
		cin >> newWidth;
		cout << "Type your new resolution height: ";
		cin >> newHeight;
		newWidthSmall = newWidth % 256;
		newWidthBig = (newWidth - newWidthSmall) / 256;
		newHeightSmall = newHeight % 256;
		newHeightBig = (newHeight - newHeightSmall) / 256;
		binaryFile.seekp(0x001448A3);
		binaryFile << char(newWidthSmall);
		binaryFile.seekp(0x001448A4);
		binaryFile << char(newWidthBig);
		binaryFile.seekp(0x001448AE);
		binaryFile << char(newHeightSmall);
		binaryFile.seekp(0x001448AF);
		binaryFile << char(newHeightBig);
		fov = (static_cast<float>(newWidth) / static_cast<float>(newHeight) * 0.375 - 0.5) / 2 + 0.5;
		//cout << float(multiplier) << "\n";
		const unsigned char * pf = reinterpret_cast<const unsigned char*>(&fov);
		for (size_t i = 0; i != sizeof(float); ++i)
		{
			binaryFile.seekp(0x001448C0+i);
			binaryFile << char(pf[i]);
			//printf("%X ", pf[i]);
		}
		cout << "Resolution changed.";
		getch();
	}
	else
	{
		cout << "Supported SecretAgent.exe file not found.";
		getch();
	}
	return 0;
}
