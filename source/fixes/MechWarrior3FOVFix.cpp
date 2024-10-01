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
const streampos kCameraHorizontalFOVOffset = 0x001CCF3C;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight;
string input;
fstream file, file2;
int choice, tempChoice;
bool fileNotFound, validKeyPressed, isGameplayHFOVKnown;
float newCameraHorizontalFOV, oldWidth = 4.0f, oldHeight = 3.0f, oldCameraHorizontalFOV, oldHorizontalCameraFOVValue, oldAspectRatio = oldWidth / oldHeight, currentFOVInDegrees, newGameplayHFOV, newCameraHorizontalFOVValue;
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
        {"\xD8\x0D\x50\xDB\x5C\x00\xD9\xC9\xD8\x0D\x50\xDB\x5C\x00", 14},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 004F3C76 | D8 0D 50 DB 5C 00 | fmul dword ptr [005CDB50]
        // 004F3C7C | D9 C9             | fxch st(1)
        // 004F3C7E | D8 0D 50 DB 5C 00 | fmul dword ptr [005CDB50]

        {"\xD8\x0D\x50\xDB\x5C\x00\xD9\x44\x24\x10", 10},
        // DISASSEMBLED CODE - PATTERN 2 (UNMODIFIED)
        // 004F3E3D | D8 0D 50 DB 5C 00 | fmul dword ptr [005CDB50]
        // 004F3E43 | D9 44 24 10       | fld dword ptr [esp+10]

        {"\xD8\x0D\x50\xDB\x5C\x00\x89\xBE\xF8\x00\x00\x00\x89\xBE\xFC\x00\x00\x00\x89\xBE\x00\x01\x00\x00\x89\xBE\x04\x01\x00\x00\xD9\xF2\xDD\xD8\xD8\x8E\xB4\x00\x00\x00\xD9\x96\x08\x01\x00\x00\xD9\x86\xEC\x00\x00\x00\xD8\x0D\x50\xDB\x5C\x00", 58}
        // DISASSEMBLED CODE - PATTERN 3 (UNMODIFIED)
        // 004F420D | D8 0D 50 DB 5C 00 | fmul dword ptr [005CDB50]
        // 004F4213 | 89 BE F8 00 00 00 | mov dword ptr [esi+F8],edi
        // 004F4219 | 89 BE FC 00 00 00 | mov dword ptr [esi+FC],edi
        // 004F421F | 89 BE 00 01 00 00 | mov dword ptr [esi+100],edi
        // 004F4225 | 89 BE 04 01 00 00 | mov dword ptr [esi+104],edi
        // 004F422B | D9 F2             | fptan
        // 004F422D | DD D8             | fstp st(0)
        // 004F422F | D8 8E B4 00 00 00 | fmul dword ptr [esi+B4]
        // 004F4235 | D9 96 08 01 00 00 | fst dword ptr [esi+108]
        // 004F423B | D9 86 EC 00 00 00 | fld dword ptr [esi+EC]
        // 004F4241 | D8 0D 50 DB 5C 00 | fmul dword ptr [005CDB50]
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xD8\x0D\x3C\xDB\x5C\x00\xD9\xC9\xD8\x0D\x3C\xDB\x5C\x00", 14},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 004F3C76 | D8 0D 3C DB 5C 00 | fmul dword ptr [005CDB3C]
        // 004F3C7C | D9 C9             | fxch st(1)
        // 004F3C7E | D8 0D 3C DB 5C 00 | fmul dword ptr [005CDB3C]

        {"\xD8\x0D\x3C\xDB\x5C\x00\xD9\x44\x24\x10", 10},
        // DISASSEMBLED CODE - PATTERN 2 (MODIFIED)
        // 004F3E3D | D8 0D 3C DB 5C 00 | fmul dword ptr [005CDB3C]
        // 004F3E43 | D9 44 24 10       | fld dword ptr [esp+10]

        {"\xD8\x0D\x3C\xDB\x5C\x00\x89\xBE\xF8\x00\x00\x00\x89\xBE\xFC\x00\x00\x00\x89\xBE\x00\x01\x00\x00\x89\xBE\x04\x01\x00\x00\xD9\xF2\xDD\xD8\xD8\x8E\xB4\x00\x00\x00\xD9\x96\x08\x01\x00\x00\xD9\x86\xEC\x00\x00\x00\xD8\x0D\x3C\xDB\x5C\x00", 58}
        // DISASSEMBLED CODE - PATTERN 3 (MODIFIED)
        // 004F420D | D8 0D 3C DB 5C 00 | fmul dword ptr [005CDB3C]
        // 004F4213 | 89 BE F8 00 00 00 | mov dword ptr [esi+F8],edi
        // 004F4219 | 89 BE FC 00 00 00 | mov dword ptr [esi+FC],edi
        // 004F421F | 89 BE 00 01 00 00 | mov dword ptr [esi+100],edi
        // 004F4225 | 89 BE 04 01 00 00 | mov dword ptr [esi+104],edi
        // 004F422B | D9 F2             | fptan
        // 004F422D | DD D8             | fstp st(0)
        // 004F422F | D8 8E B4 00 00 00 | fmul dword ptr [esi+B4]
        // 004F4235 | D9 96 08 01 00 00 | fst dword ptr [esi+108]
        // 004F423B | D9 86 EC 00 00 00 | fld dword ptr [esi+EC]
        // 004F4241 | D8 0D 3C DB 5C 00 | fmul dword ptr [005CDB3C]
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

float NewCameraHorizontalFOVCalculation(float &oldHorizontalCameraFOVValue, uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraHorizontalFOVValue = (2.0f * RadToDeg(atan((static_cast<float>(newWidthValue) / static_cast<float>(newHeightValue)) / oldAspectRatio) * tan(DegToRad(oldHorizontalCameraFOVValue / 2.0f)))) / 160.0f;
    return newCameraHorizontalFOVValue;
}

int main()
{

    cout << "MechWarrior 3 (1999) FOV Fixer v1.3 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newCameraHorizontalFOV = NewCameraHorizontalFOVCalculation(oldCameraHorizontalFOV = 80.0f, newWidth, newHeight);

        OpenFile(file, "Mech3.exe");

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

        cout << "\n---------------------------\n";
    } while (choice != 1); // Checks the flag in the loop condition
}