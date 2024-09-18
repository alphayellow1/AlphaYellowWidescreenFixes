#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <cstdint> // For uint32_t
#include <limits>
#include <windows.h>
#include <vector>
#include <cstring>
#include <conio.h>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const streampos kHFOVOffset = 0x000969C3;
const streampos kResolutionWidthOffset = 0x00000010;
const streampos kResolutionHeightOffset = 0x00000014;

// Variables
int choice, tempChoice;
uint32_t currentWidth, currentHeight, newWidth, newHeight;
float CameraHorizontalFOVValue, CameraHorizontalFOV;
bool fileNotFound, validKeyPressed;
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
            cout << "\nFailed to open " << filename << ", check if the file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the file is currently being used. Press Enter when all the mentioned problems are solved." << endl;
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

void SearchAndReplacePatterns(fstream &file)
{
    // Defines the original and new patterns with their sizes
    vector<pair<const char *, size_t>> patterns = {
        {"\x8B\x46\x30\x8D\x9E\x80\x00\x00\x00", 9},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 0043813C | 8B 46 30          | mov eax,[esi+30]
        // 0043813F | 8D 9E 80 00 00 00 | lea ebx,[esi+00000080]
        {"\xB8\x28\x94\x49\x00\xE9\xE7\x2A\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 47}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\x7F\xE8\x05\x00\x90\x90\x90\x90", 9},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 0043813C | E9 7F E8 05 00 | jmp 004969C0
        // 00438141 | 90             | nop
        // 00438142 | 90             | nop
        // 00438143 | 90             | nop
        // 00438144 | 90             | nop
        {"\xB8\x28\x94\x49\x00\xE9\xE7\x2A\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x46\x30\x00\x00\x40\x3F\x8B\x46\x30\xE9\x76\x17\xFA\xFF", 47}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 004969C0 (x32dbg)
        // 004969C0 | C7 46 30 00 00 40 3F | mov [esi+30],3F400000
        // 004969C7 | 8B 46 30             | mov eax,[esi+30]
        // 004969CA | E9 76 17 FA FF       | jmp 00438145
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

float HandleHFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    CameraHorizontalFOVValue = 0.75f / ((static_cast<float>(newWidthValue) / static_cast<float>(newHeightValue)) / (4.0f / 3.0f));

    return CameraHorizontalFOVValue;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Codename Silver (2002) Widescreen Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "silver.cfg");

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

        CameraHorizontalFOV = HandleHFOVCalculation(newWidth, newHeight);

        OpenFile(file, "Silver.exe");

        SearchAndReplacePatterns(file);

        file.seekp(kHFOVOffset);
        file.write(reinterpret_cast<const char *>(&CameraHorizontalFOV), sizeof(CameraHorizontalFOV));

        file.close();

        OpenFile(file, "silver.cfg");

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.close();

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << " and fixed the field of view." << endl;
        }
        else
        {
            cout << "\nError(s) occurred during the file operations." << endl;
        }

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice);

        if (choice == 1)
        {
            cout << "\nPress Enter to exit the program...";
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice != 1); // Checks the flag in the loop condition
}