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
const streampos kCameraFOVOffset = 0x00347628;
const streampos kWeaponFOVOffset = 0x0034762C;

// Variables
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
double oldWidth = 4.0, oldHeight = 3.0, oldHFOV, oldAspectRatio = oldWidth / oldHeight, newAspectRatio, newWidth, newHeight, newCustomResolutionValue;
float currentCameraFOV, currentWeaponFOV, newCameraFOV, newWeaponFOV, newCustomFOV;
string input, descriptor;
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

float HandleFOVInput()
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a float
        newCustomFOV = stof(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newCustomFOV <= 0 || newCustomFOV >= 180)
        {
            cout << "Please enter a valid number for the FOV (greater than 0 and less than 180)." << endl;
        }
    } while (newCustomFOV <= 0 || newCustomFOV >= 180);

    return newCustomFOV;
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

    file.open("ss.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {
        // Tries to open the file again
        file.open("ss.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open ss.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nss.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Secret Service: In Harm's Way (2001) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kCameraFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraFOV), sizeof(currentCameraFOV));

        file.seekg(kWeaponFOVOffset);
        file.read(reinterpret_cast<char *>(&currentWeaponFOV), sizeof(currentWeaponFOV));

        cout << "\n====== Your current FOVs ======" << endl;
        cout << "\nCamera FOV: " << currentCameraFOV << "\u00B0" << endl;
        cout << "Weapon FOV: " << currentWeaponFOV << "\u00B0" << endl;


        cout << "\n- Do you want to set FOV automatically based on a desired resolution (1) or set custom values for camera and weapon FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            newWidth = HandleResolutionInput();

            cout << "\n- Enter the desired height: ";
            newHeight = HandleResolutionInput();

            newAspectRatio = newWidth / newHeight;
            
            // Calculates the new camera FOV
            oldHFOV = 90.0;
            newCameraFOV = 2.0 * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0))));

            // Calculates the new weapon FOV
            oldHFOV = 60.0;
            newWeaponFOV = 2.0 * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0))));

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired camera FOV (in degrees, default for 4:3 is 90\u00B0): ";
            newCameraFOV = HandleFOVInput();

            cout << "\n- Enter the desired weapon FOV (in degrees, default for 4:3 is 60\u00B0): ";
            newWeaponFOV = HandleFOVInput();

            descriptor = "manually";

            break;
        }

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        file.seekp(kWeaponFOVOffset);
        file.write(reinterpret_cast<const char *>(&newWeaponFOV), sizeof(newWeaponFOV));

        // Confirmation message
        cout << "\nSuccessfully changed " << descriptor << " the field of view." << endl;
        cout << "\nNew Camera FOV: " << newCameraFOV << "\u00B0" << endl;
        cout << "New Weapon FOV: " << newWeaponFOV << "\u00B0" << endl;

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