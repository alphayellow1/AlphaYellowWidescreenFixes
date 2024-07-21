#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <limits>
#include <windows.h>
#include <conio.h>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const double kTolerance = 0.01;
const streampos kCameraFOV1Offset = 0x0046B927;
const streampos kCameraFOV2Offset = 0x0046B93C;
const streampos kWeaponFOVOffset = 0x0046B94E;
const streampos kAspectRatioOffset1 = 0x00011F9C;
const streampos kAspectRatioOffset2 = 0x0001236B;
const streampos kAspectRatioOffset3 = 0x00012640;
const streampos kAspectRatioOffset4 = 0x0006C65C;
const streampos kAspectRatioOffset5 = 0x0027B9F1;
const streampos kAspectRatioOffset6 = 0x002BC5B2;
const streampos kAspectRatioOffset7 = 0x0033D9F6;
const streampos kAspectRatioOffset8 = 0x0033DF54;
const streampos kAspectRatioOffset9 = 0x0034BD35;
const streampos kAspectRatioOffset10 = 0x0035083D;
const streampos kAspectRatioOffset11 = 0x005D7C40;

// Variables
int choice1, choice2, tempChoice, digitLengthWidth, digitLengthHeight;
bool fileNotFound, validKeyPressed;
double oldWidth = 4.0, oldHeight = 3.0, oldHFOV, oldAspectRatio = oldWidth / oldHeight, newAspectRatio, newWidth, newHeight, currentCameraFOVInDegrees, currentWeaponFOVInDegrees, newCameraFOVInDegrees, newWeaponFOVInDegrees, newCustomFOVInDegrees, newCustomResolutionValue;
float currentCameraFOVInRadians, currentWeaponFOVInRadians, newCameraFOVInRadians, newWeaponFOVInRadians, AspectRatioInFile;
string descriptor, input, newResolutionWidthAsString, newResolutionHeightAsString;
fstream file;
char ch;

// Function to convert degrees to radians
double DegToRad(double degrees)
{
    return degrees * (kPi / 180.0);
}

// Function to convert radians to degrees
double RadToDeg(double radians)
{
    return radians * (180.0 / kPi);
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

double HandleFOVInput()
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a float
        newCustomFOVInDegrees = stof(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newCustomFOVInDegrees <= 0 || newCustomFOVInDegrees >= 180)
        {
            cout << "Please enter a valid number for the FOV (greater than 0 and less than 180)." << endl;
        }
    } while (newCustomFOVInDegrees <= 0 || newCustomFOVInDegrees >= 180);

    return newCustomFOVInDegrees;
}

double HandleResolutionInput()
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

    return newCustomResolutionValue;
}

// Function to open the file
void OpenFile(fstream &file)
{
    fileNotFound = false;

    file.open("PaintballGame.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {
        // Tries to open the file again
        file.open("PaintballGame.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open PaintballGame.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nPaintballGame.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Splat Magazine Renegade Paintball (2005) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kCameraFOV1Offset);
        file.read(reinterpret_cast<char *>(&currentCameraFOVInRadians), sizeof(currentCameraFOVInRadians));

        file.seekg(kWeaponFOVOffset);
        file.read(reinterpret_cast<char *>(&currentWeaponFOVInRadians), sizeof(currentWeaponFOVInRadians));

        // Converts the FOV values from radians to degrees
        currentCameraFOVInDegrees = RadToDeg(static_cast<double>(currentCameraFOVInRadians));
        currentWeaponFOVInDegrees = RadToDeg(static_cast<double>(currentWeaponFOVInRadians));

        cout << "\n====== Your current FOVs ======" << endl;
        cout << "\nCamera FOV: " << currentCameraFOVInDegrees << "\u00B0" << endl;
        cout << "Weapon FOV: " << currentWeaponFOVInDegrees << "\u00B0" << endl;

        cout << "\n- Enter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\n- Enter the desired height: ";
        newHeight = HandleResolutionInput();

        newAspectRatio = newWidth / newHeight;

        cout << "\n- Do you want to set FOV automatically based on the resolution typed above (1) or set custom values for camera and weapon FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            // Calculates the new camera FOV
            oldHFOV = 90.0;
            newCameraFOVInDegrees = 2.0 * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0))));

            // Calculates the new weapon FOV
            oldHFOV = 70.0;
            newWeaponFOVInDegrees = 2.0 * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0))));

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired camera FOV (in degrees, default for 4:3 is 90\u00B0): ";
            newCameraFOVInDegrees = HandleFOVInput();

            cout << "\n- Enter the desired weapon FOV (in degrees, default for 4:3 is 70\u00B0): ";
            newWeaponFOVInDegrees = HandleFOVInput();

            descriptor = "manually";

            break;
        }

        newCameraFOVInRadians = static_cast<float>(DegToRad(newCameraFOVInDegrees)); // Converts degrees to radians

        newWeaponFOVInRadians = static_cast<float>(DegToRad(newWeaponFOVInDegrees)); // Converts degrees to radians

        AspectRatioInFile = static_cast<float>(newAspectRatio);

        file.seekp(kCameraFOV1Offset);
        file.write(reinterpret_cast<const char *>(&newCameraFOVInRadians), sizeof(newCameraFOVInRadians));

        file.seekp(kCameraFOV2Offset);
        file.write(reinterpret_cast<const char *>(&newCameraFOVInRadians), sizeof(newCameraFOVInRadians));

        file.seekp(kWeaponFOVOffset);
        file.write(reinterpret_cast<const char *>(&newWeaponFOVInRadians), sizeof(newWeaponFOVInRadians));

        file.seekp(kAspectRatioOffset1);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset2);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset3);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset4);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset5);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset6);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset7);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset8);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset9);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset10);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        file.seekp(kAspectRatioOffset11);
        file.write(reinterpret_cast<const char *>(&AspectRatioInFile), sizeof(AspectRatioInFile));

        // Confirmation message
        cout << "\nSuccessfully changed " << descriptor << " the field of view." << endl;
        cout << "\nNew Camera FOV: " << newCameraFOVInDegrees << "\u00B0" << endl;
        cout << "New Weapon FOV: " << newWeaponFOVInDegrees << "\u00B0" << endl;

        // Closes the file
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice2);

        if (choice2 == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n----------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}