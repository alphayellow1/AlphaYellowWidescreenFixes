#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <conio.h> // For getch() function [get character]
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <string>
#include <algorithm>
#include <vector>

using namespace std;

// Constants
const streampos kResolutionWidthOffset = 0x0004CF10;
const streampos kResolutionHeightOffset = 0x0004CF15;
const streampos kAspectRatioOffset = 0x001B48D4;
const streampos kCameraFOVOffset = 0x001B48E3;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight;
string input, descriptor;
fstream file;
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
float newCameraFOV, newAspectRatio, newAspectRatioValue;
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

float NewAspectRatioCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newAspectRatioValue = static_cast<float>(newWidthValue) / static_cast<float>(newHeightValue);
    return newAspectRatioValue;
}

void SearchAndReplacePatterns(fstream &file)
{
    // Defines the original and new patterns with their sizes
    vector<pair<const char *, size_t>> patterns = {
        {"\xD9\x40\xA4\xD8\x4C\x24\x28\xD9\x5C\x24\x28\xD9\x44\x24\x28\xD9\x5C\x24\x04\xD9\x40\xA0\xD9\x1C\x24", 25},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 00445372 | D9 40 A4    | fld dword ptr [eax-5C] <-- ASPECT RATIO INSTRUCTION
        // 00445375 | D8 4C 24 28 | fmul dword ptr [esp+28]
        // 00445379 | D9 5C 24 28 | fstp dword ptr [esp+28]
        // 0044537D | D9 44 24 28 | fld dword ptr [esp+28]
        // 00445381 | D9 5C 24 04 | fstp dword ptr [esp+4]
        // 00445385 | D9 40 A0    | fld dword ptr [eax-60] <-- FIELD OF VIEW INSTRUCTION
        // 00445388 | D9 1C 24    | fstp dword ptr [esp]
        {"\xFD\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 67}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\x5A\xF5\x16\x00\x90\x90\xD9\x5C\x24\x28\xD9\x44\x24\x28\xD9\x5C\x24\x04\xE9\x5A\xF5\x16\x00\x90", 25},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 00445372 | E9 5A F5 16 00 | jmp 005B48D1
        // 00445377 | 90             | nop
        // 00445378 | 90             | nop
        // 00445379 | D9 5C 24 28    | fstp dword ptr [esp+28]
        // 0044537D | D9 44 24 28    | fld dword ptr [esp+28]
        // 00445381 | D9 5C 24 04    | fstp dword ptr [esp+4]
        // 00445385 | E9 5A F5 16 00 | jmp 005B48E4
        // 0044538A | 90             | nop
        {"\xFD\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x40\xA4\x39\x8E\xE3\x3F\xD9\x40\xA4\xD8\x4C\x24\x28\xE9\x95\x0A\xE9\xFF\xC7\x40\xA0\xEF\xAD\x95\x3F\xD9\x40\xA0\xD9\x1C\x24\xE9\x95\x0A\xE9\xFF", 67}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 005B48D1 (x32dbg)
        // 005B48D1 | C7 40 A4 39 8E E3 3F | mov dword ptr [eax-5C],3FE38E39
        // 005B48D8 | D9 40 A4             | fld dword ptr [eax-5C]
        // 005B48DB | D8 4C 24 28          | fmul dword ptr [esp+28]
        // 005B48DF | E9 99 0A E9 FF       | jmp 00445379
        // 005B48E4 | C7 40 A0 EF AD 95 3F | mov dword ptr [eax-60],3F95ADEF
        // 005B48EB | D9 40 A0             | fld dword ptr [eax-60]
        // 005B48EE | D9 1C 24             | fstp dword ptr [esp]
        // 005B48F1 | E9 95 0A E9 FF       | jmp 0044538B
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

int main()
{
    cout << "Barbie Horse Adventures: Riding Camp (2008) Widescreen Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "PXGameStudioRuntime2008.exe");

        file.seekg(kResolutionWidthOffset);
        file.read(reinterpret_cast<char *>(&currentWidth), sizeof(currentWidth));

        file.seekg(kResolutionHeightOffset);
        file.read(reinterpret_cast<char *>(&currentHeight), sizeof(currentHeight));

        cout << "\nCurrent resolution is " << currentWidth << "x" << currentHeight << endl;

        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newAspectRatio = NewAspectRatioCalculation(newWidth, newHeight);

        cout << "\n- Do you want to set a custom camera FOV value (1) or leave it as default (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired field of view multiplier (default value is 1.169370532): ";
            HandleFOVInput(newCameraFOV);
            break;

        case 2:
            newCameraFOV = 1.169370532f;
            break;
        }

        SearchAndReplacePatterns(file);

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << " and fixed the field of view." << endl;
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

        cout << "\n-----------------------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}