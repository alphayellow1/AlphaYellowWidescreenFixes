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
const streampos kHipfireFOV1Offset = 0x00213F87;
const streampos kHipfireFOV2Offset = 0x00213FA3;
const streampos kAimDownSightsFOVOffset = 0x00213F8E;

// Variables
int choice1, choice2, tempChoice, digitLengthWidth, digitLengthHeight;
bool fileNotFound, validKeyPressed;
double oldWidth = 4.0, oldHeight = 3.0, oldHFOV, oldAspectRatio = oldWidth / oldHeight, newAspectRatio, newWidth, newHeight, currentHipfireFOVInDegrees, currentAimDownSightsFOVInDegrees, newHipfireFOVInDegrees, newAimDownSightsFOVInDegrees, newCustomFOVInDegrees, newCustomResolutionValue;
float currentHipfireFOVInRadians, currentAimDownSightsFOVInRadians, newHipfireFOVInRadians, newAimDownSightsFOVInRadians;
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

    file.open("CloakNTEngine.dll", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {
        // Tries to open the file again
        file.open("CloakNTEngine.dll", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open CloakNTEngine.dll, check if the DLL has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently being used. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nCloakNTEngine.dll opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "History Civil War: Secret Missions (2008) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kHipfireFOV1Offset);
        file.read(reinterpret_cast<char *>(&currentHipfireFOVInRadians), sizeof(currentHipfireFOVInRadians));

        file.seekg(kAimDownSightsFOVOffset);
        file.read(reinterpret_cast<char *>(&currentAimDownSightsFOVInRadians), sizeof(currentAimDownSightsFOVInRadians));

        // Converts the FOV values from radians to degrees
        currentHipfireFOVInDegrees = RadToDeg(static_cast<double>(currentHipfireFOVInRadians));
        currentAimDownSightsFOVInDegrees = RadToDeg(static_cast<double>(currentAimDownSightsFOVInRadians));

        cout << "\n====== Your current FOVs ======" << endl;
        cout << "\nHipfire FOV: " << currentHipfireFOVInDegrees << "\u00B0" << endl;
        cout << "Aim Down Sights FOV: " << currentAimDownSightsFOVInDegrees << "\u00B0" << endl;

        cout << "\n- Do you want to set FOV automatically based on a desired resolution (1) or set custom values for hipfire and aim down sights FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            newWidth = HandleResolutionInput();

            cout << "\n- Enter the desired height: ";
            newHeight = HandleResolutionInput();

            newAspectRatio = newWidth / newHeight;

            // Calculates the new hipfire FOV
            oldHFOV = 70.000007;
            newHipfireFOVInDegrees = 2.0 * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0))));

            // Calculates the new aim down sights FOV
            oldHFOV = 50.000004;
            newAimDownSightsFOVInDegrees = 2.0 * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0))));

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired hipfire FOV (in degrees, default for 4:3 is 70\u00B0): ";
            newHipfireFOVInDegrees = HandleFOVInput();

            cout << "\n- Enter the desired aim down sights FOV (in degrees, default for 4:3 is 50\u00B0): ";
            newAimDownSightsFOVInDegrees = HandleFOVInput();

            descriptor = "manually";

            break;
        }

        newHipfireFOVInRadians = static_cast<float>(DegToRad(newHipfireFOVInDegrees)); // Converts degrees to radians

        newAimDownSightsFOVInRadians = static_cast<float>(DegToRad(newAimDownSightsFOVInDegrees)); // Converts degrees to radians

        file.seekp(kHipfireFOV1Offset);
        file.write(reinterpret_cast<const char *>(&newHipfireFOVInRadians), sizeof(newHipfireFOVInRadians));

        file.seekp(kHipfireFOV2Offset);
        file.write(reinterpret_cast<const char *>(&newHipfireFOVInRadians), sizeof(newHipfireFOVInRadians));

        file.seekp(kAimDownSightsFOVOffset);
        file.write(reinterpret_cast<const char *>(&newAimDownSightsFOVInRadians), sizeof(newAimDownSightsFOVInRadians));

        // Confirmation message
        cout << "\nSuccessfully changed " << descriptor << " the field of view." << endl;
        cout << "\nNew Hipfire FOV: " << newHipfireFOVInDegrees << "\u00B0" << endl;
        cout << "New Aim Down Sights FOV: " << newAimDownSightsFOVInDegrees << "\u00B0" << endl;

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