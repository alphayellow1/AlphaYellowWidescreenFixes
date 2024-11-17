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
const streampos kCameraHorizontalFOVOffset = 0x0020E9A2;

// Variables
uint32_t newWidth, newHeight;
string input;
fstream file;
int choice, tempChoice;
bool fileNotFound, validKeyPressed;
double originalAspectRatio = 5.0 / 4.0; // 5:4
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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
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
        {"\xD9\x05\x00\x62\x61\x00\xD8\xC9\xD9\x5C\x24\x08", 12},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 00472123 | D9 05 00 62 61 00 | fld dword ptr [00616200]
        // 00472129 | D8 C9             | fmul st(0),st(1)
        // 0047212B | D9 5C 24 08       | fstp dword ptr [esp+8]

        {"\xB9\xA4\x12\x69\x00\xE9\x86\xFA\xE5\xFF\x90\x90\x90\x90\x90\x90\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 34}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xD9\x05\x00\x62\x61\x00\xE9\x62\xC8\x19\x00\x90", 12},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 00472123 | D9 05 00 62 61 00 | fld dword ptr [00616200]
        // 00472129 | E9 62 C8 19 00    | jmp 0060E990
        // 0047212E | 90                | nop

        {"\xB9\xA4\x12\x69\x00\xE9\x86\xFA\xE5\xFF\x90\x90\x90\x90\x90\x90\xD8\xC9\xD8\x0D\xA2\xE9\x60\x00\x3E\xD9\x5C\x24\x08\xE9\x8D\x37\xE6\xFF", 34}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 0060E990 (x32dbg)
        // 0060E990 | D8 C9             | fmul st(0),st(1)
        // 0060E992 | D8 0D A2 E9 60 00 | fmul dword ptr [0060E9A2]
        // 0060E998 | 3E D9 5C 24 08    | fstp dword ptr [esp+8]
        // 0060E99D | E9 8D 37 E6 FF    | jmp 0047212F
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
    return (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (originalAspectRatio);
}

int main()
{

    cout << "Airborne Troops: Countdown to D-Day (2004) FOV Fixer v1.0 by AlphaYellow and AuToMaNiAk005, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        OpenFile(file, "AirborneTroops.exe");

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