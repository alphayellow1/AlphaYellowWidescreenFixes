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
const streampos kCameraHorizontalFOVOffset = 0x004D10FB;

// Variables
uint32_t newWidth, newHeight;
string input;
fstream file;
int choice, tempChoice;
bool fileNotFound, validKeyPressed;
float newCameraHorizontalFOV;
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
        {"\x8B\x2D\x88\xB7\x95\x00\xD9\x05\x10\xBA\x95\x00", 12},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 0072FA8E | 8B 2D 88 B7 95 00 | mov ebp, dword ptr [0095B788]
        // 0072FA94 | D9 05 10 BA 95 00 | fld dword ptr [0095BA10]

        {"\xD8\x4D\x74\xD9\x05\x10\xBA\x95\x00", 9},
        // DISASSEMBLED CODE - PATTERN 2 (UNMODIFIED)
        // 00734369 | D8 4D 74          | fmul dword ptr [ebp+74]
        // 0073436C | D9 05 10 BA 95 00 | fld dword ptr [0095BA10]

        {"\x8B\x1D\x78\xB7\x95\x00\xD9\x05\x10\xBA\x95\x00", 12}
        // DISASSEMBLED CODE - PATTERN 3 (UNMODIFIED)
        // 0073484C | 8B 1D 78 B7 95 00 | mov ebx, dword ptr [0095B778]
        // 00734852 | D9 05 10 BA 95 00 | fld dword ptr [0095BA10]
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\x8B\x2D\x88\xB7\x95\x00\xD9\x05\xFB\x1C\x8D\x00", 12},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 0072FA8E | 8B 2D 88 B7 95 00 | mov ebp, dword ptr [0095B788]
        // 0072FA94 | D9 05 FB 1C 8D 00 | fld dword ptr [008D1CFB]

        {"\xD8\x4D\x74\xD9\x05\xFB\x1C\x8D\x00", 9},
        // DISASSEMBLED CODE - PATTERN 2 (MODIFIED)
        // 00734369 | D8 4D 74          | fmul dword ptr [ebp+74]
        // 0073436C | D9 05 FB 1C 8D 00 | fld dword ptr [008D1CFB]

        {"\x8B\x1D\x78\xB7\x95\x00\xD9\x05\xFB\x1C\x8D\x00", 12}
        // DISASSEMBLED CODE - PATTERN 3 (MODIFIED)
        // 0073484C | 8B 1D 78 B7 95 00 | mov ebx, dword ptr [0095B778]
        // 00734852 | D9 05 FB 1C 8D 00 | fld dword ptr [008D1CFB]
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

double NewCameraHorizontalFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    return (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0);
}

int main()
{

    cout << "Adrenalin: Extreme Show (2005) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        OpenFile(file, "Adrenalin.exe");

        SearchAndReplacePatterns(file);

        newCameraHorizontalFOV = static_cast<float>(NewCameraHorizontalFOVCalculation(newWidth, newHeight));

        file.seekp(kCameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOV), sizeof(newCameraHorizontalFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully fixed the field of view." << endl;
        }
        else
        {
            cout << "\nError(s) occurred during the file operations." << endl;
        }

        // Closes the file
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice);

        if (choice == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice != 1); // Checks the flag in the loop condition
}