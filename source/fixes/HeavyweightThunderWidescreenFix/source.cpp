#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <conio.h> // For getch() function [get character]
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <string>
#include <cstring>
#include <algorithm>

using namespace std;

// Variables
uint32_t iCurrentWidth, iCurrentHeight, iNewWidth, iNewHeight;
string input, descriptor1, descriptor2;
fstream file;
streampos spResolutionWidth640Offset, spResolutionHeight480Offset, spResolutionWidth800Offset, spResolutionHeight600Offset, spResolutionWidth1024Offset, spResolutionHeight768Offset, spResolutionWidth1280Offset, spResolutionHeight1024Offset, spCameraFOVOffset;
int iChoice1, iChoice2, iTempChoice;
bool bFileNotFound, bValidKeyPressed;
float fNewCameraFOV, fNewCameraFOVValue, fNewAspectRatio;
char cCh;

constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Function to handle user input in choices
void HandleChoiceInput(int& choice)
{
	iTempChoice = -1;         // Temporary variable to store the input
	bValidKeyPressed = false; // Flag to track if a valid key was pressed

	while (true)
	{
		cCh = _getch(); // Waits for user to press a key

		// Checks if the key is '1' or '2'
		if ((cCh == '1' || cCh == '2') && !bValidKeyPressed)
		{
			iTempChoice = cCh - '0';  // Converts char to int and stores the input temporarily
			cout << cCh;             // Echoes the valid input
			bValidKeyPressed = true; // Sets the flag as a valid key has been pressed
		}
		else if (cCh == '\b' || cCh == 127) // Handles backspace or delete keys
		{
			if (iTempChoice != -1) // Checks if there is something to delete
			{
				iTempChoice = -1;         // Resets the temporary choice
				cout << "\b \b";         // Erases the last character from the console
				bValidKeyPressed = false; // Resets the flag as the input has been deleted
			}
		}
		// If 'Enter' is pressed and a valid key has been pressed prior
		else if (cCh == '\r' && bValidKeyPressed)
		{
			choice = iTempChoice; // Assigns the temporary input to the choice variable
			cout << endl;        // Moves to a new line
			break;               // Exits the loop since we have a confirmed input
		}
	}
}

// Function to handle user input in resolution
uint32_t HandleResolutionInput()
{
	uint32_t newCustomResolutionValue;

	do
	{
		cin >> newCustomResolutionValue;

		cin.clear();                                         // Clears error flags
		cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

		if (cin.fail())
		{
			cin.clear();                                         // Clears error flags
			cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
			cout << "Invalid input. Please enter a numeric value." << endl;
		}
		else if (newCustomResolutionValue <= 0 || newCustomResolutionValue >= 65535)
		{
			cout << "Please enter a valid number." << endl;
		}
	} while (newCustomResolutionValue <= 0 || newCustomResolutionValue > 65535);

	return newCustomResolutionValue;
}

// Function to open the file
void OpenFile(fstream& file, const string& filename)
{
	bFileNotFound = false;

	file.open(filename, ios::in | ios::out | ios::binary);

	// If the file is not open, sets fileNotFound to true
	if (!file.is_open())
	{
		bFileNotFound = true;
	}

	// Loops until the file is found and opened
	while (bFileNotFound)
	{
		// Tries to open the file again
		file.open(filename, ios::in | ios::out | ios::binary);

		if (!file.is_open())
		{
			cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
			do
			{
				cCh = _getch(); // Waits for user to press a key
			} while (cCh != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
		}
		else
		{
			cout << "\n" << filename << " opened successfully!" << endl;
			bFileNotFound = false; // Sets fileNotFound to false as the file is found and opened
		}
	}
}

int main()
{
	cout << "Heavyweight Thunder (2005) Widescreen Fixer v1.0 by AlphaYellow, 2025\n\n----------------\n";

	do
	{
		OpenFile(file, "boxing.exe");

		spResolutionWidth640Offset = 0x000654E4;

		spResolutionHeight480Offset = 0x000654EE;

		spResolutionWidth800Offset = 0x000654FA;

		spResolutionHeight600Offset = 0x00065504;

		spResolutionWidth1024Offset = 0x00065510;

		spResolutionHeight768Offset = 0x0006551A;

		spResolutionWidth1280Offset = 0x00065526;

		spResolutionHeight1024Offset = 0x00065530;

		cout << "\n- Enter the desired width: ";
		iNewWidth = HandleResolutionInput();

		cout << "\n- Enter the desired height: ";
		iNewHeight = HandleResolutionInput();

		file.seekp(spResolutionWidth640Offset);
		file.write(reinterpret_cast<const char*>(&iNewWidth), sizeof(iNewWidth));

		file.seekp(spResolutionHeight480Offset);
		file.write(reinterpret_cast<const char*>(&iNewHeight), sizeof(iNewHeight));

		file.seekp(spResolutionWidth800Offset);
		file.write(reinterpret_cast<const char*>(&iNewWidth), sizeof(iNewWidth));

		file.seekp(spResolutionHeight600Offset);
		file.write(reinterpret_cast<const char*>(&iNewHeight), sizeof(iNewHeight));

		file.seekp(spResolutionWidth1024Offset);
		file.write(reinterpret_cast<const char*>(&iNewWidth), sizeof(iNewWidth));

		file.seekp(spResolutionHeight768Offset);
		file.write(reinterpret_cast<const char*>(&iNewHeight), sizeof(iNewHeight));

		file.seekp(spResolutionWidth1280Offset);
		file.write(reinterpret_cast<const char*>(&iNewWidth), sizeof(iNewWidth));

		file.seekp(spResolutionHeight1024Offset);
		file.write(reinterpret_cast<const char*>(&iNewHeight), sizeof(iNewHeight));

		// Checks if any errors occurred during the file operations
		if (file.good())
		{
			// Confirmation message
			cout << "\nSuccessfully changed the resolution to " << iNewWidth << "x" << iNewHeight << "." << endl;
		}
		else
		{
			cout << "\nError(s) occurred during the file operations." << endl;
		}

		// Closes the file
		file.close();

		cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
		HandleChoiceInput(iChoice2);

		if (iChoice2 == 1)
		{
			cout << "\nPress Enter to exit the program...";
			do
			{
				cCh = _getch(); // Waits for user to press a key
			} while (cCh != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
			return 0;
		}

		cout << "\n-----------------------------------------\n";
	} while (iChoice2 != 1); // Checks the flag in the loop condition
}