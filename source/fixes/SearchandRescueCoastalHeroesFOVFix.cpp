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
const streampos kOutsideCameraFOVOffset = 0x00001AB2;
const streampos kCockpitCameraFOVOffset = 0x00001A2A;
const streampos kHoistCameraFOVOffset = 0x00001B3D;
const streampos kFlyByCameraFOVOffset = 0x0011B2F0;

// Variables
int choice1, choice2, tempChoice, digitLengthWidth, digitLengthHeight;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
double currentOutsideCameraFOVInRadians, currentCockpitCameraFOVInRadians, currentHoistCameraFOVInRadians, currentFlyByCameraFOVInRadians, 
newOutsideCameraFOVInRadians, newCockpitCameraFOVInRadians, newHoistCameraFOVInRadians, newFlyByCameraFOVInRadians, 
oldWidth = 4.0, oldHeight = 3.0, oldCameraHorizontalFOV, oldAspectRatio = oldWidth / oldHeight, currentOutsideCameraFOVInDegrees, 
currentCockpitCameraFOVInDegrees, currentHoistCameraFOVInDegrees, currentFlyByCameraFOVInDegrees, newOutsideCameraFOVInDegrees, newCockpitCameraFOVInDegrees, 
newHoistCameraFOVInDegrees, newFlyByCameraFOVInDegrees, newCameraFOVInDegreesValue;
string descriptor, input, r;
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

void HandleFOVInput(double &newCustomFOVInDegrees)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
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

double NewCameraFOVInDegreesCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue, double &oldCameraHorizontalFOVValue)
{
    newCameraFOVInDegreesValue = 2.0 * RadToDeg(atan((static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / oldAspectRatio) * tan(DegToRad(oldCameraHorizontalFOVValue / 2.0)));
    return newCameraFOVInDegreesValue;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Search & Rescue: Coastal Heroes (2002) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "Sar4.exe");

        file.seekg(kOutsideCameraFOVOffset);
        file.read(reinterpret_cast<char *>(&currentOutsideCameraFOVInRadians), sizeof(currentOutsideCameraFOVInRadians));

        file.seekg(kCockpitCameraFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCockpitCameraFOVInRadians), sizeof(currentCockpitCameraFOVInRadians));

        file.seekg(kHoistCameraFOVOffset);
        file.read(reinterpret_cast<char *>(&currentHoistCameraFOVInRadians), sizeof(currentHoistCameraFOVInRadians));

        file.seekg(kFlyByCameraFOVOffset);
        file.read(reinterpret_cast<char *>(&currentFlyByCameraFOVInRadians), sizeof(currentFlyByCameraFOVInRadians));

        // Converts the FOV values from radians to degrees
        currentOutsideCameraFOVInDegrees = RadToDeg(currentOutsideCameraFOVInRadians);
        currentCockpitCameraFOVInDegrees = RadToDeg(currentCockpitCameraFOVInRadians);
        currentHoistCameraFOVInDegrees = RadToDeg(currentHoistCameraFOVInRadians);
        currentFlyByCameraFOVInDegrees = RadToDeg(currentFlyByCameraFOVInRadians);

        cout << "\n====== Current FOVs ======" << endl;
        cout << "\nOutside Camera FOV: " << currentOutsideCameraFOVInDegrees << "\u00B0" << endl;
        cout << "Cockpit Camera FOV: " << currentCockpitCameraFOVInDegrees << "\u00B0" << endl;
        cout << "Hoist Camera FOV: " << currentHoistCameraFOVInDegrees << "\u00B0" << endl;
        cout << "Fly-By Camera FOV: " << currentFlyByCameraFOVInDegrees << "\u00B0" << endl;

        cout << "\n- Do you want to set FOV automatically based on a desired resolution (1) or set custom FOV values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            // Calculates the new FOV for the outside camera
            newOutsideCameraFOVInDegrees = NewCameraFOVInDegreesCalculation(newWidth, newHeight, oldCameraHorizontalFOV = 75.0);

            // Calculates the new FOV for the cockpit camera
            newCockpitCameraFOVInDegrees = NewCameraFOVInDegreesCalculation(newWidth, newHeight, oldCameraHorizontalFOV = 90.0);

            // Calculates the new FOV for the hoist camera
            newHoistCameraFOVInDegrees = NewCameraFOVInDegreesCalculation(newWidth, newHeight, oldCameraHorizontalFOV = 90.0);

            // Calculates the new FOV for the fly-by camera
            newFlyByCameraFOVInDegrees = NewCameraFOVInDegreesCalculation(newWidth, newHeight, oldCameraHorizontalFOV = 1.0);

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired outside camera FOV (in degrees, default for 4:3 is 75\u00B0): ";
            HandleFOVInput(newOutsideCameraFOVInDegrees);

            cout << "\n- Enter the desired cockpit camera FOV (in degrees, default for 4:3 is 90\u00B0): ";
            HandleFOVInput(newCockpitCameraFOVInDegrees);

            cout << "\n- Enter the desired hoist camera FOV (in degrees, default for 4:3 is 90\u00B0): ";
            HandleFOVInput(newHoistCameraFOVInDegrees);

            cout << "\n- Enter the desired fly-by camera FOV (in degrees, default for 4:3 is 1\u00B0): ";
            HandleFOVInput(newFlyByCameraFOVInDegrees);

            descriptor = "manually";

            break;
        }

        newOutsideCameraFOVInRadians = DegToRad(newOutsideCameraFOVInDegrees); // Converts degrees to radians

        newCockpitCameraFOVInRadians = DegToRad(newCockpitCameraFOVInDegrees); // Converts degrees to radians

        newHoistCameraFOVInRadians = DegToRad(newHoistCameraFOVInDegrees); // Converts degrees to radians

        newFlyByCameraFOVInRadians = DegToRad(newFlyByCameraFOVInDegrees); // Converts degrees to radians

        file.seekp(kOutsideCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newOutsideCameraFOVInRadians), sizeof(newOutsideCameraFOVInRadians));

        file.seekp(kCockpitCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCockpitCameraFOVInRadians), sizeof(newCockpitCameraFOVInRadians));

        file.seekp(kHoistCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newHoistCameraFOVInRadians), sizeof(newHoistCameraFOVInRadians));

        file.seekp(kFlyByCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newFlyByCameraFOVInRadians), sizeof(newFlyByCameraFOVInRadians));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed " << descriptor << " the field of view." << endl;
            cout << "\nNew Outside Camera FOV: " << newOutsideCameraFOVInDegrees << "\u00B0" << endl;
            cout << "New Cockpit Camera FOV: " << newCockpitCameraFOVInDegrees << "\u00B0" << endl;
            cout << "New Hoist Camera FOV: " << newHoistCameraFOVInDegrees << "\u00B0" << endl;
            cout << "New Fly-By Camera FOV: " << newFlyByCameraFOVInDegrees << "\u00B0" << endl;
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