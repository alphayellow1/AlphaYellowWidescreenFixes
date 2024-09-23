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
const float kPi = 3.14159265358979323846f;
const float kTolerance = 0.01f;
const streampos kCameraFOVOffset = 0x00347628;
const streampos kWeaponFOVOffset = 0x0034762C;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float currentCameraFOV, currentWeaponFOV, newCameraFOV, newWeaponFOV, newCameraFOVValue, oldWidth = 4.0f, oldHeight = 3.0f, oldCameraHorizontalFOV, oldAspectRatio = oldWidth / oldHeight;
string input, descriptor;
fstream file;
char ch;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
    return degrees * (kPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
    return radians * (180.0f / kPi);
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

void HandleFOVInput(float &newCustomFOV)
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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
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

float NewFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue, float &oldCameraHorizontalFOVValue)
{
    newCameraFOVValue = 2.0f * RadToDeg(atan((static_cast<float>(newWidthValue) / static_cast<float>(newHeightValue)) / oldAspectRatio) * tan(DegToRad(oldCameraHorizontalFOVValue / 2.0f)));
    return newCameraFOVValue;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Secret Service: In Harm's Way (2001) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "ss.exe");

        file.seekg(kCameraFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraFOV), sizeof(currentCameraFOV));

        file.seekg(kWeaponFOVOffset);
        file.read(reinterpret_cast<char *>(&currentWeaponFOV), sizeof(currentWeaponFOV));

        cout << "\n====== Current FOVs ======" << endl;
        cout << "\nCamera FOV: " << currentCameraFOV << "\u00B0" << endl;
        cout << "Weapon FOV: " << currentWeaponFOV << "\u00B0" << endl;

        cout << "\n- Do you want to set FOV automatically based on a desired resolution (1) or set custom values for camera and weapon FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            // Calculates the new camera FOV
            newCameraFOV = NewFOVCalculation(newWidth, newHeight, oldCameraHorizontalFOV = 90.0f);

            // Calculates the new weapon FOV
            newWeaponFOV = NewFOVCalculation(newWidth, newHeight, oldCameraHorizontalFOV = 60.0f);

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired camera FOV (in degrees, default for 4:3 is 90\u00B0): ";
            HandleFOVInput(newCameraFOV);

            cout << "\n- Enter the desired weapon FOV (in degrees, default for 4:3 is 60\u00B0): ";
            HandleFOVInput(newWeaponFOV);

            descriptor = "manually";

            break;
        }

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        file.seekp(kWeaponFOVOffset);
        file.write(reinterpret_cast<const char *>(&newWeaponFOV), sizeof(newWeaponFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed " << descriptor << " the field of view." << endl;
            cout << "\nNew Camera FOV: " << newCameraFOV << "\u00B0" << endl;
            cout << "New Weapon FOV: " << newWeaponFOV << "\u00B0" << endl;
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
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n-----------------------------------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}