#include <conio.h>
#include <direct.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <locale.h>
#include <cstdint>
#include <stdint.h>
#include <windows.h>
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kCheck1Offset = 0x000568B2;
const streampos kCheck2Offset = 0x000568B3;
const streampos kCompassNeedleWidthOffset = 0x0019D0F8;
const streampos kFOVOffset = 0x0019D0FC;
const streampos kLoadingBarXOffset = 0x001A707C;
const streampos kRedCrossAndMenuTextWidthOffset = 0x001A7084;
const streampos kHUDLeftMarginOffset = 0x001A7128;
const streampos kCompassNeedleXOffset = 0x001A8200;
const streampos kObjectiveDirectionWidthOffset = 0x001A8220;
const streampos kCompassXOffset = 0x001A822C;
const streampos kCompassWidthOffset = 0x001A8234;
const streampos kWeaponsListWidthOffset = 0x001A8244;
const streampos kHUDTextWidthOffset = 0x001A8328;
const streampos kAmmoXOffset = 0x001A8354;
const streampos kAmmoWidthOffset = 0x001A835C;
const streampos kNewWidthSmall1Offset = 0x0009345A;
const streampos kNewWidthBig1Offset = 0x0009345B;
const streampos kNewHeightSmall1Offset = 0x00093462;
const streampos kNewHeightBig1Offset = 0x00093463;
const streampos kNewWidthSmall2Offset = 0x00013D70;
const streampos kNewWidthBig2Offset = 0x00013D71;
const streampos kNewHeightSmall2Offset = 0x00013D78;
const streampos kNewHeightBig2Offset = 0x00013D79;

// Variables
bool isFileCorrect, fileNotFound, validKeyPressed, openedSuccessfullyMessage;
int tempChoice, language, choice, incorrectCount;
uint8_t newWidthSmall, newWidthBig, newHeightSmall, newHeightBig, check1, check2;
uint32_t newWidth, newHeight;
float newAspectRatio, hudWidth, hudMargin, fov, fovMultiplier, flt;
double dbl;
char ch;
fstream file;
string input;

// Function to handle user input in choices
void HandleChoiceInput(int &choice)
{
	tempChoice = -1;		 // Temporary variable to store the input
	validKeyPressed = false; // Flag to track if a valid key was pressed

	while (true)
	{
		ch = _getch(); // Waits for user to press a key

		// Checks if the key is '1' or '2'
		if ((ch == '1' || ch == '2') && !validKeyPressed)
		{
			tempChoice = ch - '0';	// Converts char to int and stores the input temporarily
			cout << ch;				// Echoes the valid input
			validKeyPressed = true; // Sets the flag as a valid key has been pressed
		}
		else if (ch == '\b' || ch == 127) // Handles backspace or delete keys
		{
			if (tempChoice != -1) // Checks if there is something to delete
			{
				tempChoice = -1;		 // Resets the temporary choice
				cout << "\b \b";		 // Erases the last character from the console
				validKeyPressed = false; // Resets the flag as the input has been deleted
			}
		}
		// If 'Enter' is pressed and a valid key has been pressed prior
		else if (ch == '\r' && validKeyPressed)
		{
			choice = tempChoice; // Assigns the temporary input to the choice variable
			cout << endl;		 // Moves to a new line
			break;				 // Exits the loop since we have a confirmed input
		}
	}
}

void HandleFOVInput(float &newCustomFOVMultiplier)
{
	do
	{
		// Reads the input as a string
		cin >> input;

		// Replaces all commas with dots
		replace(input.begin(), input.end(), ',', '.');

		// Parses the string to a float
		newCustomFOVMultiplier = stof(input);

		if (cin.fail())
		{
			cin.clear();										 // Clears error flags
			cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
			cout << "Invalid input. Please enter a numeric value." << endl;
		}
		else if (newCustomFOVMultiplier <= 0 || newCustomFOVMultiplier >= 180)
		{
			cout << "Please enter a valid number for the FOV (greater than 0 and less than 180)." << endl;
		}
	} while (newCustomFOVMultiplier <= 0 || newCustomFOVMultiplier >= 180);
}

// Function to handle user input in resolution
void HandleResolutionWidthInput(uint32_t &newCustomResolutionWidthValue)
{
	do
	{
		cin >> newCustomResolutionWidthValue;

		cin.clear();										 // Clears error flags
		cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

		if (cin.fail())
		{
			cin.clear();										 // Clears error flags
			cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
			cout << "Invalid input. Please enter a numeric value." << endl;
		}
		else if (newCustomResolutionWidthValue < 0)
		{
			if (language == 1)
			{
				cout << "Did you really type a negative number?" << endl;
			}
			else
			{
				cout << "Naprawd� wpisa�e� liczb� ujemn�?" << endl;
			}
		}
		else if (newCustomResolutionWidthValue == 0)
		{
			if (language == 1)
			{
				cout << "Are you kidding me?" << endl;
			}
			else
			{
				cout << "Jaja sobie robisz?" << endl;
			}
		}
		else if (newCustomResolutionWidthValue > 65535)
		{
			if (language == 1)
			{
				cout << "Back in 2024, we weren't using screens with such high resolutions and I was too lazy to add support for such high values. Please type a lower resolution (less than 65536 of width and height) which matches your target aspect ratio and put your values with a hex editor at 0009345A (width) and 00093462 (height) offsets." << endl;
			}
			else
			{
				cout << "W 2024 roku, nie korzystali�my z ekran�w o tak wysokiej rozdzielczo�ci, a ja by�em zbyt leniwy, by doda� wsparcie dla tak wysokich warto�ci. Prosz� poda� ni�sz� rozdzielczo�� (mniejsza od 65536 w szeroko�ci i wysoko�ci) kt�ra jest zgodna w proporcjami Twojego ekranu i umie�ci� swoje warto�ci za pomoc� edytora szesnastkowego w offsetach 0009345A (szeroko��) i 00093462 (wysoko��)." << endl;
			}
		}
	} while (newCustomResolutionWidthValue <= 0 || newCustomResolutionWidthValue > 65535);
}

void HandleResolutionHeightInput(uint32_t &newCustomResolutionHeightValue)
{
	do
	{
		cin >> newCustomResolutionHeightValue;

		cin.clear();										 // Clears error flags
		cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

		if (cin.fail())
		{
			cin.clear();										 // Clears error flags
			cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
			cout << "Invalid input. Please enter a numeric value." << endl;
		}
		else if (newCustomResolutionHeightValue < 0)
		{
			if (language == 1)
			{
				cout << "Why don't you just flip your screen upside down?" << endl;
			}
			else
			{
				cout << "Dlaczego po prostu nie odwr�cisz swojego ekranu do g�ry nogami?" << endl;
			}
		}
		else if (newCustomResolutionHeightValue == 0)
		{
			if (language == 1)
			{
				cout << "No." << endl;
			}
			else
			{
				cout << "Nie." << endl;
			}
		}
		else if (newCustomResolutionHeightValue > 65535)
		{
			if (language == 1)
			{
				cout << "Back in 2024, we weren't using screens with such high resolutions and I was too lazy to add support for such high values. Please type a lower resolution (less than 65536 of width and height) which matches your target aspect ratio and put your values with a hex editor at 0009345A (width) and 00093462 (height) offsets." << endl;
			}
			else
			{
				cout << "W 2024 roku, nie korzystali�my z ekran�w o tak wysokiej rozdzielczo�ci, a ja by�em zbyt leniwy, by doda� wsparcie dla tak wysokich warto�ci. Prosz� poda� ni�sz� rozdzielczo�� (mniejsza od 65536 w szeroko�ci i wysoko�ci) kt�ra jest zgodna w proporcjami Twojego ekranu i umie�ci� swoje warto�ci za pomoc� edytora szesnastkowego w offsetach 0009345A (szeroko��) i 00093462 (wysoko��)." << endl;
			}
		}
	} while (newCustomResolutionHeightValue <= 0 || newCustomResolutionHeightValue > 65535);
}

// Function to open the file
void OpenFile(fstream &file, const string &filename)
{
	fileNotFound = false;

	file.open(filename, ios::in | ios::out | ios::binary);

	// If the file is not open, sets fileNotFound to true
	if (!file.is_open())
	{
		fileNotFound = true;
	}

	// Loops until the file is found and opened
	while (fileNotFound)
	{
		// Tries to open the file again
		file.open(filename, ios::in | ios::out | ios::binary);

		if (!file.is_open())
		{
			cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
			do
			{
				ch = _getch(); // Waits for user to press a key
			} while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
		}
		else
		{
			cout << "\n"
				 << filename << " opened successfully!" << endl;
			fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
		}
	}
}

int main()
{
	cout << "Beyond Normandy: Assignment Berlin Widescreen Fixer v1.1 by AuToMaNiAk005 and AlphaYellow, 2024\n\n----------------\n\n";

	cout << "1: English\n";

	cout << "2: Polski\n";

	cout << "\n----------------\n\n";

	HandleChoiceInput(language);

	cout << "\n----------------\n";

	do
	{
		incorrectCount = 0;

		do
		{
			OpenFile(file, "AssignmentBerlin-V1.05.exe");

			file.seekg(kCheck1Offset);
			file.read(reinterpret_cast<char *>(&check1), sizeof(check1));

			file.seekg(kCheck2Offset);
			file.read(reinterpret_cast<char *>(&check2), sizeof(check2));

			if (check1 == 0xF8 && check2 == 0xD0)
			{
				isFileCorrect = true;
			}
			else
			{
				isFileCorrect = false;
			}

			if (!isFileCorrect)
			{
				incorrectCount++; // Increment counter if file is incorrect

				file.close();

				if (language == 1)
				{
					cout << "\nUnsupported .exe file.\nMake sure you've put the included AssignmentBerlin-V1.05.exe file in the game directory, press Enter to try again." << endl;
					do
					{
						ch = _getch(); // Waits for user to press a key
					} while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
				}
				else if (language == 2)
				{
					setlocale(LC_CTYPE, "Polish");
					cout << "\nNieobs�ugiwany plik .exe.\nUpewnij si�, �e umie�ci�e� za��czony plik AssignmentBerlin-V1.05.exe w folderze z gr�, naciśnij Enter, aby spróbować ponownie." << endl;
					do
					{
						ch = _getch(); // Waits for user to press a key
					} while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
				}
			}
		} while (!isFileCorrect);

		if (incorrectCount > 0)
		{
			if (language == 1)
			{
				cout << "\nAssignmentBerlin-V1.05.exe opened successfully!" << endl;
			}
			else if (language == 2)
			{
				setlocale(LC_CTYPE, "Polish");
				cout << "\nAssignmentBerlin-V1.05.exe został pomyślnie otwarty." << endl;
			}
		}

		if (language == 1)
		{
			cout << "\n- Type your resolution width: ";
			HandleResolutionWidthInput(newWidth);

			cout << "\n- Type your resolution height: ";
			HandleResolutionHeightInput(newHeight);
		}
		else if (language == 2)
		{
			setlocale(LC_CTYPE, "Polish");

			cout << "- Wpisz szeroko�� rozdzielczo�ci: ";
			HandleResolutionWidthInput(newWidth);

			cout << "- Wpisz wysoko�� rozdzielczo�ci: ";
			HandleResolutionHeightInput(newHeight);
		}

		newWidthSmall = newWidth % 256;

		newWidthBig = (newWidth - newWidthSmall) / 256;

		newHeightSmall = newHeight % 256;

		newHeightBig = (newHeight - newHeightSmall) / 256;

		newAspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);

		hudWidth = 600.0f * newAspectRatio;

		if (language == 1)
		{
			cout << "\n- Type your FOV multiplier (1 = default; at high values, the character's hands may appear unfinished): ";
			HandleFOVInput(fovMultiplier);

			cout << "\n- Type your HUD margin (0 = default, " << hudWidth / 2 - 400 << " = centered to 4:3): ";
			cin >> hudMargin;
		}
		else if (language == 2)
		{
			setlocale(LC_CTYPE, "Polish");

			cout << "\n- Wpisz mno�nik pola widzenia (1 = domy�lny, przy wysokich warto�ciach r�ce postaci mog� wygl�da� na niekompletne): ";
			HandleFOVInput(fovMultiplier);

			cout << "\n- Wpisz margines dla interfejsu (0 = domy�lny, " << hudWidth / 2 - 400 << " = wy�rodkowany do 4:3): ";
			cin >> hudMargin;
		}

		file.seekp(kNewWidthSmall1Offset);
		file.write(reinterpret_cast<const char *>(&newWidthSmall), sizeof(newWidthSmall));

		file.seekp(kNewWidthBig1Offset);
		file.write(reinterpret_cast<const char *>(&newWidthBig), sizeof(newWidthBig));

		file.seekp(kNewHeightSmall1Offset);
		file.write(reinterpret_cast<const char *>(&newHeightSmall), sizeof(newHeightSmall));

		file.seekp(kNewHeightBig1Offset);
		file.write(reinterpret_cast<const char *>(&newHeightBig), sizeof(newHeightBig));

		file.seekp(kNewWidthSmall2Offset);
		file.write(reinterpret_cast<const char *>(&newWidthSmall), sizeof(newWidthSmall));

		file.seekp(kNewWidthBig2Offset);
		file.write(reinterpret_cast<const char *>(&newWidthBig), sizeof(newWidthBig));

		file.seekp(kNewHeightSmall2Offset);
		file.write(reinterpret_cast<const char *>(&newHeightSmall), sizeof(newHeightSmall));

		file.seekp(kNewHeightBig2Offset);
		file.write(reinterpret_cast<const char *>(&newHeightBig), sizeof(newHeightBig));

		// Compass needle width
		flt = 0.0333333f / newAspectRatio;
		file.seekp(kCompassNeedleWidthOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Field of view
		flt = 0.75f * newAspectRatio * fovMultiplier;
		file.seekp(kFOVOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Loading bar X
		flt = 222.0f + (hudWidth - 800.0f) / 2.0f;
		file.seekp(kLoadingBarXOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Red cross and menu text width
		flt = 0.001666f / newAspectRatio;
		file.seekp(kRedCrossAndMenuTextWidthOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// HUD left margin
		flt = 0.0333333f / newAspectRatio + hudMargin / hudWidth;
		file.seekp(kHUDLeftMarginOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Compass needle X
		flt = (hudWidth - 79.0f - hudMargin) / hudWidth;
		file.seekp(kCompassNeedleXOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Objective direction width
		dbl = 0.001666 / static_cast<double>(newAspectRatio);
		file.seekp(kObjectiveDirectionWidthOffset);
		file.write(reinterpret_cast<const char *>(&dbl), sizeof(dbl));

		// Compass X
		flt = (hudWidth - 118.0f - hudMargin) / hudWidth;
		file.seekp(kCompassXOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Compass width
		flt = 0.163333f / newAspectRatio;
		file.seekp(kCompassWidthOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Weapons list width
		flt = 0.17f / newAspectRatio;
		file.seekp(kWeaponsListWidthOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// HUD text width
		dbl = 0.0006666666666 / static_cast<double>(newAspectRatio);
		file.seekp(kHUDTextWidthOffset);
		file.write(reinterpret_cast<const char *>(&dbl), sizeof(dbl));

		// Ammo X
		flt = (hudWidth - 74.0f - hudMargin) / hudWidth;
		file.seekp(kAmmoXOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Ammo width
		flt = 0.116666f / newAspectRatio;
		file.seekp(kAmmoWidthOffset);
		file.write(reinterpret_cast<const char *>(&flt), sizeof(flt));

		// Checks if any errors occurred during the file operations
		if (file.good())
		{
			if (language == 1)
			{
				cout << "\nAssignmentBerlin-V1.05.exe has been patched." << endl;
			}
			else if (language == 2)
			{
				cout << "\nAssignmentBerlin-V1.05.exe zosta� zaktualizowany." << endl;
			}
		}
		else
		{
			if (language == 1)
			{
				cout << "\nError(s) occurred during the file operations." << endl;
			}
			else if (language == 2)
			{
				cout << "\nWystąpił(y) błąd podczas operacji na plikach." << endl;
			}
			
		}

		file.close();

		if (language == 1)
		{
			cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
		}
		else if (language == 2)
		{
			cout << "\n- Czy chcesz wyjść z programu (1) czy wypróbować inną wartość (2)?: ";
		}

		HandleChoiceInput(choice);

		if (choice == 1)
		{
			if (language == 1)
			{
				cout << "\nPress Enter to exit the program...";
			}
			else if (language == 2)
			{
				cout << "\nNaciśnij Enter, aby wyjść z programu...";
			}

			do
			{
				ch = _getch(); // Waits for user to press a key
			} while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
			return 0;
		}

		cout << "\n----------------------------\n";
	} while (choice == 2); // Checks the flag in the loop condition
}