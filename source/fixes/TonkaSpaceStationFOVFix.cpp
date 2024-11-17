#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch() function [get character]
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <string>
#include <algorithm>
#include <cstring>
#include <vector>

using namespace std;

// Constants
const streampos kCameraHorizontalFOVOffset = 0x0005BCAA;

// Variables
int choice, tempChoice;
uint32_t newWidth, newHeight;
fstream file;
float newCameraHorizontalFOV;
bool fileNotFound, validKeyPressed;
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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
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
        {"\xD8\x4A\x34\xD9\x42\x30", 6},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 00438C57 | D8 4A 34 | fmul dword ptr [edx+34]
        // 00438C5A | D9 42 30 | fld dword ptr [edx+30]

        {"\x83\xC8\xFF\xC3\x33\xC0\xC3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 25}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\x4B\x30\x02\x00\x90", 6},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 00438C57 | E9 4B 30 02 00 | jmp 0045BCA7
        // 00438C5C | 90             | nop

        {"\x83\xC8\xFF\xC3\x33\xC0\xC3\xC7\x42\x34\x00\x00\x40\x3F\xD8\x4A\x34\xD9\x42\x30\xE9\xA4\xCF\xFD\xFF", 25}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 0045BCA7 (x32dbg)
        // 0045BCA7 | C7 42 34 00 00 40 3F | mov [edx+34],3F400000
        // 0045BCAE | D8 4A 34             | fmul dword ptr [edx+34]
        // 0045BCB1 | D9 42 30             | fld dword ptr [edx+30]
        // 0045BCB4 | E9 A4 CF FD FF       | jmp 00438C5D
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
    return ((static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0)) * 0.75;
}

int main()
{
    cout << "Tonka Space Station (2000) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newCameraHorizontalFOV = static_cast<float>(NewCameraHorizontalFOVCalculation(newWidth, newHeight));

        OpenFile(file, "SpaceStation.exe");

        SearchAndReplacePatterns(file);

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

        cout << "\n-----------------------------------------\n";
    } while (choice == 2); // Checks the flag in the loop condition
}