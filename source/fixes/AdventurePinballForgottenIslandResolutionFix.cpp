#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <windows.h>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <cstring>
#include <algorithm>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const streampos k640ResolutionOffset = 0x000063C9;
const streampos k480ResolutionOffset = 0x000063D4;
const streampos k800ResolutionOffset = 0x000063DB;
const streampos k600ResolutionOffset = 0x000063E4;
const streampos k1024ResolutionOffset1 = 0x000063EB;
const streampos k768ResolutionOffset = 0x000063F4;
const streampos k1280ResolutionOffset = 0x000063FB;
const streampos k1024ResolutionOffset2 = 0x00006404;
const streampos k1600ResolutionOffset = 0x0000640B;
const streampos k1200ResolutionOffset = 0x00006414;

// Variables
int choice1, choice2, tempChoice, simplifiedWidth, simplifiedHeight, aspectRatioGCD;
uint32_t newWidth, newHeight, newCustomResolutionValue;
bool fileNotFound, validKeyPressed;
double newAspectRatio, standardRatio, newWidthInDouble, newHeightInDouble;
string input;
fstream file;
char ch;

// Function to calculate the greatest common divisor
int gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
}

// Function to handle user input in choices
void HandleChoiceInput(int &choice)
{
    tempChoice = -1;         // Temporary variable to store the input
    validKeyPressed = false; // Flag to track if a valid key was pressed

    while (true)
    {
        ch = _getch(); // Waits for user to press a key

        // Checks if the key is '1' or '2'
        if ((ch == '1' || ch == '2') && !validKeyPressed)
        {
            tempChoice = ch - '0';  // Converts char to int and stores the input temporarily
            cout << ch;             // Echoes the valid input
            validKeyPressed = true; // Sets the flag as a valid key has been pressed
        }
        else if (ch == '\b' || ch == 127) // Handles backspace or delete keys
        {
            if (tempChoice != -1) // Checks if there is something to delete
            {
                tempChoice = -1;         // Resets the temporary choice
                cout << "\b \b";         // Erases the last character from the console
                validKeyPressed = false; // Resets the flag as the input has been deleted
            }
        }
        // If 'Enter' is pressed and a valid key has been pressed prior
        else if (ch == '\r' && validKeyPressed)
        {
            choice = tempChoice; // Assigns the temporary input to the choice variable
            cout << endl;        // Moves to a new line
            break;               // Exits the loop since we have a confirmed input
        }
    }
}

void HandleResolutionInput(uint32_t &newCustomResolutionValue)
{
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
            cout << "\nFailed to open " << filename << ", check if the DLL has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently being used. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\n" << filename << " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "Adventure Pinball: Forgotten Island (2001) Resolution Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        TCHAR buffer[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, buffer);

        do
        {
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            // Calculates the greatest common divisor of the width and height
            aspectRatioGCD = gcd(static_cast<int>(newWidth), static_cast<int>(newHeight));

            // Simplifies the width and height by dividing by the greatest common divisor
            simplifiedWidth = static_cast<int>(newWidth) / aspectRatioGCD;
            simplifiedHeight = static_cast<int>(newHeight) / aspectRatioGCD;

            if (simplifiedWidth == 16 && simplifiedHeight == 3)
            {
                simplifiedWidth = 48;
                simplifiedHeight = 9;
            }
            else if (simplifiedWidth == 8 && simplifiedHeight == 5)
            {
                simplifiedWidth = 16;
                simplifiedHeight = 10;
            }
            else if (simplifiedWidth == 7 && simplifiedHeight == 3)
            {
                simplifiedWidth = 21;
                simplifiedHeight = 9;
            }
            else if (simplifiedWidth == 5 && simplifiedHeight == 1)
            {
                simplifiedWidth = 45;
                simplifiedHeight = 9;
            }

            newAspectRatio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

            standardRatio = 16.0 / 9.0; // 16:9 aspect ratio

            // Initializes choice1 to 1 to allow exiting the loop if aspect ratio is not wider than 16:9
            choice1 = 1;

            // Checks if the calculated aspect ratio is wider than 16:9
            if (newAspectRatio > standardRatio)
            {
                cout << "\nWarning: The aspect ratio of " << newWidth << "x" << newHeight << ", which is " << simplifiedWidth << ":" << simplifiedHeight << ", is wider than 16:9, menus will be too cropped and become unusable! Are you sure you want to proceed (1) or want to insert a less wider resolution closer to 16:9 (2)?: ";
                HandleChoiceInput(choice1);
            }
        } while (choice1 != 1);

        OpenFile(file, "D3DDrv.dll");

        file.seekp(k640ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth)); // 640x480

        file.seekp(k480ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));
        
        file.seekp(k800ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth)); // 800x600

        file.seekp(k600ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(k1024ResolutionOffset1);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth)); // 1024x768

        file.seekp(k768ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(k1280ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth)); // 1280x1024

        file.seekp(k1024ResolutionOffset2);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(k1600ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth)); // 1600x1200

        file.seekp(k1200ResolutionOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            cout << "\nSuccessfully changed the resolution. Now open PB.ini inside " << buffer << "\\ and set FullscreenViewportX=" << newWidth << " and FullscreenViewportY=" << newHeight << " under the [WinDrv.WindowsClient] section." << endl;
        }
        else
        {
            cout << "\nError(s) occurred during the file operations." << endl;
        }

        // Closes the file
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice2);

        if (choice2 == 1)
        {
            cout << "\nPress Enter to exit the program...";
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}