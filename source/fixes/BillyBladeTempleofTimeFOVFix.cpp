#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <windows.h>
#include <vector>
#include <cstring>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const streampos kAspectRatioOffset = 0x000A5AE4;
const streampos kCameraFOVOffset = 0x000A5AF3;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
double newCameraFOV, newAspectRatio;
string input;
fstream file;
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

void HandleFOVInput(double &newCustomFOV)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
        newCustomFOV = stof(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newCustomFOV <= 0)
        {
            cout << "Please enter a valid number for the FOV multiplier (greater than 0)." << endl;
        }
    } while (newCustomFOV <= 0);
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
        {"\xD9\x46\x34\xD9\x1C\x24\x51\xD9\x46\x38\xD9\x1C\x24", 13},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 0043B73D | D9 46 34 | fld dword ptr [esi+34] <-- ASPECT RATIO INSTRUCTION
        // 0043B740 | D9 1C 24 | fstp dword ptr [esp]
        // 0043B743 | 51       | push ecx
        // 0043B744 | D9 46 38 | fld dword ptr [esi+38] <-- FIELD OF VIEW INSTRUCTION
        // 0043B747 | D9 1C 24 | fstp dword ptr [esp]
        {"\xC3\xB8\xFC\xB2\x4A\x00\xE9\x5E\x3C\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 63}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\x9F\xA3\x06\x00\x90\x51\xE9\xA7\xA3\x06\x00\x90", 13},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 0043B73D | E9 9F A3 06 00 | jmp 004A5AE1
        // 0043B742 | 90             | nop
        // 0043B743 | 51             | push ecx
        // 0043B744 | E9 A7 A3 06 00 | jmp 004A5AF0
        // 0043B749 | 90             | nop
        {"\xC3\xB8\xFC\xB2\x4A\x00\xE9\x5E\x3C\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x46\x34\xAB\xAA\xAA\x3F\xD9\x46\x34\xE9\x53\x5C\xF9\xFF\xC7\x46\x38\xAB\x61\x1C\x3F\xD9\x46\x38\xE9\x4B\x5C\xF9\xFF", 63}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 004A5AE1 (x32dbg)
        // 004A5AE1 | C7 46 34 AB AA AA 3F | mov dword ptr [esi+34],3FAAAAAB
        // 004A5AE8 | D9 46 34             | fld dword ptr [esi+34] <-- ASPECT RATIO INSTRUCTION
        // 004A5AEB | E9 53 5C F9 FF       | jmp 0043B743
        // 004A5AF0 | C7 46 38 AB 61 1C 3F | mov dword ptr [esi+38],3F1C61AB
        // 004A5AF7 | D9 46 38             | fld dword ptr [esi+38] <-- FIELD OF VIEW INSTRUCTION
        // 004A5AFA | E9 4B 5C F9 FF       | jmp 0043B74A
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
    SetConsoleOutputCP(CP_UTF8);

    cout << "Billy Blade: Temple of Time (2005) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newAspectRatio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

        cout << "\n- Do you want to set a custom FOV multiplier as well (1) or keep the original one (FOV is already fixed with the new aspect ratio) (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired FOV multiplier value (default value for 4:3 aspect ratio is 0,6108652949): ";

            HandleFOVInput(newCameraFOV);

            break;

        case 2:
            newCameraFOV = 0.6108652949;
            
            break;
        }

        OpenFile(file, "BillyBlade.exe");

        SearchAndReplacePatterns(file);

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            cout << "\nSuccessfully fixed the field of view." << endl;
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

        cout << "\n---------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}