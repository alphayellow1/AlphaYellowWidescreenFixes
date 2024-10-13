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
const streampos kCameraFOVOffset = 0x000DE230;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight;
string input, descriptor1, descriptor2;
fstream file;
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
float newAspectRatioAsFloat, newCameraFOVasFloat;
double newCameraFOV, newCameraFOVValue, newAspectRatio;
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
            cout << "\n" << filename << " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

void SearchAndReplacePatternsF1EXE(fstream &file)
{
    // Defines the original and new patterns with their sizes
    vector<pair<const char *, size_t>> patterns = {
        {"\x75\xF9\x2B\xC1\x48\xC3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 36}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\x75\xF9\x2B\xC1\x48\xC3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x3F", 36}
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

void SearchAndReplacePatternsD3DDLL(fstream &file)
{
    // Defines the original and new patterns with their sizes
    vector<pair<const char *, size_t>> patterns = {
        {"\xDD\xD8\xD9\x44\x24\x1C", 6},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 1000289F | DD D8       | fstp st(0)
        // 100028A1 | D9 44 24 1C | fld dword ptr [esp+1C]

        {"\x8B\x45\x08\x59\x5F\x5E\xC9\xC3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 44}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\xF8\x1A\x01\x00\x90", 6},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 1000289F | E9 F8 1A 01 00 | jmp 1001439C
        // 100028A4 | 90             | nop

        {"\x8B\x45\x08\x59\x5F\x5E\xC9\xC3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xDD\xD8\xD8\x0D\x30\xE2\x4D\x00\x3E\xD9\x44\x24\x1C\xE9\xF7\xE4\xFE\xFF", 44}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 1001439C (x32dbg)
        // 1001439C | DD D8             | fstp st(0)
        // 1001439E | D8 0D 30 E2 4D 00 | fmul dword ptr [004DE230]
        // 100143A4 | 3E D9 44 24 1C    | fld dword ptr [esp+1C]
        // 100143A9 | E9 F7 E4 FE FF    | jmp 100028A5
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

double NewCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraFOVValue = (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0);
    return newCameraFOVValue;
}

int main()
{
    cout << "F1 World Grand Prix 2000 (2001) Widescreen Fixer v1.0 by AlphaYellow and AuToMaNiAk005, 2024\n\n----------------\n";

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

        OpenFile(file, "F1.exe");

        SearchAndReplacePatternsF1EXE(file);

        file.close();

        OpenFile(file, "d3d.dll");

        SearchAndReplacePatternsD3DDLL(file);

        file.close();

        OpenFile(file, "../Data/enum.dat");

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.close();

        OpenFile(file, "F1.exe");
        
        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << " and " << descriptor1 << " the field of view" << descriptor2 << endl;
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