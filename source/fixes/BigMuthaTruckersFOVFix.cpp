#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <conio.h> // For getch() function [get character]
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <vector>
#include <cstring>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const float kPi = 3.14159265358979323846f;
const float kTolerance = 0.01f;
const streampos kAspectRatioOffset = 0x001B42E3;
const streampos kCameraFOVOffset = 0x001B42F2;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight;
string input, descriptor;
fstream file;
int choice1, choice2, tempChoice;
float newAspectRatio, newCameraFOV, newCameraFOVValue;
bool fileNotFound, validKeyPressed;
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

void HandleFOVInput(float &customFOV)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a float
        customFOV = stof(input);

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

    return customFOV;
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
            cout << "\n" << filename << " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

void SearchAndReplacePatterns(fstream &file)
{
    // Defines the original and new patterns with their sizes
    vector<pair<const char *, size_t>> patterns = {
        {"\xD8\x3D\x9C\x55\x5B\x00\xD9\x42\x44\xD9\x42\x3C", 12},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 004DE7B7 | D8 3D 9C 55 5B 00 | fdivr dword ptr [005B559C]
        // 004DE7BD | D9 42 44          | fld dword ptr [edx+44]
        // 004DE7C0 | D9 42 3C          | fld dword ptr [edx+3C]
        {"\x10\x40\x84\xD2\x75\xF9\x2B\xC1\x48\xC3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 50}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xD8\x3D\xF2\x42\x5B\x00\xE9\x1E\x5B\x0D\x00\x90", 12},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 004DE7B7 | D8 3D F2 42 5B 00 | fdivr dword ptr [005B42F2]
        // 004DE7BD | E9 1E 5B 0D 00    | jmp 005B42E0
        // 004DE7C2 | 90                | nop
        {"\x10\x40\x84\xD2\x75\xF9\x2B\xC1\x48\xC3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x42\x44\xAB\xAA\xAA\x40\xD9\x42\x44\xD9\x42\x3C\xE9\xD1\xA4\xF2\xFF", 50}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 005B42E0 (x32dbg)
        // 005B42E0 | C7 42 44 AB AA AA 40 | mov dword ptr [edx+44],40AAAAAB
        // 005B42E7 | D9 42 44             | fld dword ptr [edx+44]
        // 005B42EA | D9 42 3C             | fld dword ptr [edx+3C]
        // 005B42ED | E9 D1 A4 F2 FF       | jmp 004DE7C3
    };

    // Reads the entire file content into memory
    file.seekg(0, ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, ios::beg);
    char *buffer = new char[fileSize];
    file.read(buffer, fileSize);

    // Iterates through each pattern
    for (size_t i = 0; i < patterns.size(); ++i)
    {
        const char *originalPattern = patterns[i].first;
        size_t patternSize = patterns[i].second;
        const char *newPattern = replacements[i].first;
        size_t newPatternSize = replacements[i].second;

        // Searches for the pattern
        char *patternLocation = search(buffer, buffer + fileSize, originalPattern, originalPattern + patternSize);

        // If the pattern is found, replaces it
        if (patternLocation != buffer + fileSize)
        {
            memcpy(patternLocation, newPattern, newPatternSize);

            // Writes the modified content back to the file
            file.seekp(patternLocation - buffer);
            file.write(newPattern, newPatternSize);
        }
    }

    // Cleans up
    delete[] buffer;
    file.flush();
}

float NewCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraFOVValue = (4.0f / 3.0f) / (static_cast<float>(newWidth) / static_cast<float>(newHeight));
    return newCameraFOVValue;
}

int main()
{
    cout << "Big Mutha Truckers (2003) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newAspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);

        cout << "\n- Do you want to fix the camera FOV automatically based on the resolution typed above (1) or set a custom multiplier value for camera FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            newCameraFOV = NewCameraFOVCalculation(newWidth, newHeight);

            descriptor = "fixed";
            
            break;

        case 2:
            cout << "\n- Type a custom camera FOV multiplier value (default for 4:3 aspect ratio is 1.0): ";

            HandleFOVInput(newCameraFOV);

            descriptor = "changed";

            break;
        }

        OpenFile(file, "bmt.exe");

        SearchAndReplacePatterns(file);

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            cout << "\nSuccessfully " << descriptor << " the field of view." << endl;
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
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}