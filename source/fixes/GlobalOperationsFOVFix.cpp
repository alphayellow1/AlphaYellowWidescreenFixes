#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type 
#include <cmath>
#include <limits>
#include <windows.h>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const double kTolerance = 0.01;
const streampos kCameraHorizontalFOVOffset = 0x00117F61;
const streampos kWeaponHorizontalFOVOffset = 0x00117F71;

// Variables
int choice, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed, isFOVKnown;
double newWeaponModelHorizontalFOV, newCameraHorizontalFOV, newAspectRatio;
fstream file;
char ch;

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

    file.open("globalops.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {
        // Tries to open the file again
        file.open("globalops.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open globalops.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nglobalops.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "Global Operations (2002) FOV Fixer v1.1 by AlphaYellow, 2024\n\n----------------" << endl;

    do
    {
        do
        {
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            newAspectRatio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

            if (newAspectRatio == 1.25) // 5:4
            {
                newCameraHorizontalFOV = 1.60239994525909;
                newWeaponModelHorizontalFOV = 0.5235001445;
                isFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 1.33333333) < kTolerance) // 4:3
            {
                newCameraHorizontalFOV = 1.5707963268;
                newWeaponModelHorizontalFOV = 0.5;
                isFOVKnown = true;
            }
            else if (newAspectRatio == 1.6) // 16:10
            {
                newCameraHorizontalFOV = 1.48000001907349;
                newWeaponModelHorizontalFOV = 0.4372;
                isFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 1.77777777) < kTolerance) // 16:9
            {
                newCameraHorizontalFOV = 1.42750000953674;
                newWeaponModelHorizontalFOV = 0.4035000503;
                isFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 2.370370370370) < kTolerance) // 21:9 (2560x1080)
            {
                newCameraHorizontalFOV = 1.28869998455048;
                newWeaponModelHorizontalFOV = 0.3205520213;
                isFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 3.555555555) < kTolerance) // 32:9
            {
                newCameraHorizontalFOV = 1.118420005;
                newWeaponModelHorizontalFOV = 0.2479600161;
                isFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 5.333333333) < kTolerance) // 48:9
            {
                newCameraHorizontalFOV = 0.9877000451;
                newWeaponModelHorizontalFOV = 0.183000043;
                isFOVKnown = true;
            }
            else
            {
                cout << "\nThis aspect ratio isn't yet supported by the fixer, please contact AlphaYellow on PCGamingWiki or Discord (alphayellow) to add support for it." << endl;
                isFOVKnown = false;
            }
        } while (!isFOVKnown);

        // Opens the file
        OpenFile(file, "globalops.exe");

        // Writes the new camera HorizontalFOV and weapon HorizontalFOV values into pre-defined offset addresses in globalops.exe
        file.seekp(kCameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOV), sizeof(newCameraHorizontalFOV));

        file.seekp(kWeaponHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newWeaponModelHorizontalFOV), sizeof(newWeaponModelHorizontalFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully fixed the field of view." << endl;
        }
        else
        {
            cout << "\nError(s) occurred during the file operations." << endl;
        }

        // Closes the file
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice);

        if (choice == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n-----------------------------------------\n";
    } while (choice == 2); // Checks the flag in the loop condition
}