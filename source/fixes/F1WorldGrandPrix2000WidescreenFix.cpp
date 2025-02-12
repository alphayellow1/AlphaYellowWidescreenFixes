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
#include <vector>

using namespace std;

// Constants
const streampos kResolutionWidthOffset = 0x0000018E;
const streampos kResolutionHeightOffset = 0x00000192;
const streampos kCameraFOVOffset = 0x000DE21C;
const streampos kC1Offset = 0x000DE220;
const streampos kC2Offset = 0x000DE224;
const streampos kC3Offset = 0x000DE228;
const streampos kC4Offset = 0x000DE22C;
const streampos kC5Offset = 0x000DE230;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight;
string input, descriptor1, descriptor2;
fstream file;
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
float newAspectRatioAsFloat, newCameraFOVasFloat;
double newCameraFOV, newAspectRatio;
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

void HandleFOVInput(double &customFOV)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
        customFOV = stod(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (customFOV <= 0)
        {
            cout << "Please enter a valid number for the FOV multiplier (greater than 0)." << endl;
        }
    } while (customFOV <= 0);
}

// Function to handle user input in resolution
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

double NewCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    return (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0);
}

int main()
{
    cout << "F1 World Grand Prix 2000 (2001) Widescreen Fixer v1.2 by AlphaYellow and AuToMaNiAk005, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "../Data/enum.dat");

        file.seekg(kResolutionWidthOffset);
        file.read(reinterpret_cast<char *>(&currentWidth), sizeof(currentWidth));

        file.seekg(kResolutionHeightOffset);
        file.read(reinterpret_cast<char *>(&currentHeight), sizeof(currentHeight));

        file.close();

        cout << "\nCurrent resolution is " << currentWidth << "x" << currentHeight << "." << endl;

        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        cout << "\n- Do you want to set the field of view automatically based on the resolution set above (1) or set a custom field of view multiplier value (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            newCameraFOV = NewCameraFOVCalculation(newWidth, newHeight);
            descriptor1 = "fixed";
            descriptor2 = ".";
            break;

        case 2:
            cout << "\n- Enter the desired field of view multiplier (default value for the 4:3 aspect ratio is 1.0): ";
            HandleFOVInput(newCameraFOV);
            descriptor1 = "changed";
            descriptor2 = " to " + to_string(newCameraFOV) + ".";
            break;
        }

        newCameraFOVasFloat = static_cast<float>(newCameraFOV);

        OpenFile(file, "../Data/enum.dat");

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.close();

        OpenFile(file, "F1.exe");

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        float c1 = 4.0f / 3.0f;
        file.seekp(kC1Offset);
        file.write(reinterpret_cast<const char *>(&c1), sizeof(c1));

        float c2 = (static_cast<float>(newWidth) / static_cast<float>(newHeight)) * 0.75f;
        file.seekp(kC2Offset);
        file.write(reinterpret_cast<const char *>(&c2), sizeof(c2));

        float c3 = (static_cast<float>(newWidth) - static_cast<float>(newHeight) / 0.75f) / 2.0f;
        file.seekp(kC3Offset);
        file.write(reinterpret_cast<const char *>(&c3), sizeof(c3));

        float c4 = static_cast<float>(newWidth) - static_cast<float>(newHeight) / 0.75f;
        file.seekp(kC4Offset);
        file.write(reinterpret_cast<const char *>(&c4), sizeof(c4));

        uint32_t c5 = (600 * newWidth / newHeight - 800) / 2;
        file.seekp(kC5Offset);
        file.write(reinterpret_cast<const char *>(&c5), sizeof(c5));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << ", fixed the HUD and " << descriptor1 << " the field of view" << descriptor2 << endl;
        }
        else
        {
            cout << "\nError(s) occurred during the file operations." << endl;
        }

        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice2);

        if (choice2 == 1)
        {
            cout << "\nPress Enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n-----------------------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}