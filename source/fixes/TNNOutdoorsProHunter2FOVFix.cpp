#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type
#include <cmath>
#include <windows.h>
#include <limits>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const double kTolerance = 0.01;
const double kDefaultCameraVerticalFOVInRadians = 1.3089969158172607; // 75º
const streampos kCameraHorizontalFOVOffset = 0x0007B646;
const streampos kCameraVerticalFOVOffset = 0x0007B666;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
double currentCameraHorizontalFOVInRadians, currentCameraVerticalFOVInRadians, currentCameraHorizontalFOVInDegrees, currentCameraVerticalFOVInDegrees, newCameraHorizontalFOVInRadians, newCameraVerticalFOVInRadians, newCameraHorizontalFOVInDegrees, newCameraHorizontalFOVInDegreesValue, newCameraVerticalFOVInDegrees, oldWidth = 4.0, oldHeight = 3.0, oldCameraHorizontalFOV = 90.0, oldAspectRatio = oldWidth / oldHeight;
string descriptor, fovDescriptor, input;
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
            cout << "\nFailed to open " << filename << ", check if the DLL file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the DLL is currently being used. Press Enter when all the mentioned problems are solved." << endl;
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

double NewHorizontalCameraFOVInDegreesCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraHorizontalFOVInDegreesValue = 2.0 * RadToDeg(atan((static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / oldAspectRatio) * tan(DegToRad(oldCameraHorizontalFOV / 2.0)));
    return newCameraHorizontalFOVInDegreesValue;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "TNN Outdoors Pro Hunter 2 (1999) FOV Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "CLIENT.EXE");

        file.seekg(kCameraHorizontalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraHorizontalFOVInRadians), sizeof(currentCameraHorizontalFOVInRadians));

        file.seekg(kCameraVerticalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraVerticalFOVInRadians), sizeof(currentCameraVerticalFOVInRadians));

        file.close();

        // Converts the FOV values from radians to degrees
        currentCameraHorizontalFOVInDegrees = RadToDeg(currentCameraHorizontalFOVInRadians);
        currentCameraVerticalFOVInDegrees = RadToDeg(currentCameraVerticalFOVInRadians);

        cout << "\nCurrent FOV: " << currentCameraHorizontalFOVInDegrees << "\u00B0 (Horizontal); " << currentCameraVerticalFOVInDegrees << "\u00B0 (Vertical)\n";

        cout << "\n- Do you want to set FOV automatically based on the desired resolution (1) or set custom horizontal and vertical FOV values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            // Calculates the new horizontal FOV
            newCameraHorizontalFOVInDegrees = NewHorizontalCameraFOVInDegreesCalculation(newWidth, newHeight);

            newCameraHorizontalFOVInRadians = DegToRad(newCameraHorizontalFOVInDegrees); // Converts degrees to radians

            newCameraVerticalFOVInRadians = kDefaultCameraVerticalFOVInRadians;

            newCameraVerticalFOVInDegrees = RadToDeg(newCameraVerticalFOVInRadians);

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired horizontal FOV (in degrees, default for 4:3 is 90\u00B0): ";
            HandleFOVInput(newCameraHorizontalFOVInDegrees);

            cout << "\n- Enter the desired vertical FOV (in degrees, default for 4:3 is 75\u00B0): ";
            HandleFOVInput(newCameraVerticalFOVInDegrees);

            newCameraHorizontalFOVInRadians = DegToRad(newCameraHorizontalFOVInDegrees); // Converts degrees to radians

            newCameraVerticalFOVInRadians = DegToRad(newCameraVerticalFOVInDegrees); // Converts degrees to radians

            descriptor = "manually";

            break;
        }

        OpenFile(file, "CLIENT.EXE");

        file.seekp(kCameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOVInRadians), sizeof(newCameraHorizontalFOVInRadians));

        file.seekp(kCameraVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraVerticalFOVInRadians), sizeof(newCameraVerticalFOVInRadians));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed " << descriptor << " the horizontal FOV to " << newCameraHorizontalFOVInDegrees << "\u00B0 and vertical FOV to " << newCameraVerticalFOVInDegrees << "\u00B0." << endl;
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
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n-----------------------------------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}