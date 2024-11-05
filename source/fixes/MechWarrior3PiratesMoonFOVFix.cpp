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
const double kPi = 3.14159265358979323846;
const double kTolerance = 0.01;
const streampos kCameraHorizontalFOVOffset = 0x001DDA20;

// Variables
uint32_t newWidth, newHeight;
string input;
fstream file, file2;
int choice, tempChoice;
bool fileNotFound, validKeyPressed;
float newCameraHorizontalFOV;
double oldWidth = 4.0, oldHeight = 3.0, oldCameraHorizontalFOV, tanValue, newCameraHorizontalInDegrees, newAspectRatio, oldAspectRatio = oldWidth / oldHeight, newCameraHorizontalFOVValue;
char ch;

// Function to convert degrees to radians
double DegToRad(double degrees)
{
    return degrees * (kPi / 180.0);
}

// Function to convert radians to degrees
double RadToDeg(double radians)
{
    return radians * (180.0 / kPi);
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
        {"\xD8\x0D\x18\xE8\x5D\x00\xD9\x90\xF0\x00\x00\x00", 12},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 00507291 | D8 0D 18 E8 5D 00 | fmul dword ptr [005DE818]
        // 00507297 | D9 90 F0 00 00 00 | fst dword ptr [eax+F0]

        {"\xC7\x81\x38\x01\x00\x00\x01\x00\x00\x00\xD8\x0D\x18\xE8\x5D\x00\xC7\x81\xF8\x00\x00\x00\x01\x00\x00\x00", 26},
        // DISASSEMBLED CODE - PATTERN 2 (UNMODIFIED)
        // 005070E7 | C7 81 38 01 00 00 01 00 00 00 | mov dword ptr [ecx+138],1
        // 005070F1 | D8 0D 18 E8 5D 00             | fmul dword ptr [005DE818]
        // 005070F7 | C7 81 F8 00 00 00 01 00 00 00 | mov dword ptr [ecx+F8],1

        {"\xD9\x81\xEC\x00\x00\x00\xD9\x81\xEC\x00\x00\x00\xD9\x54\x24\x08", 16},
        // DISASSEMBLED CODE - PATTERN 3 (UNMODIFIED)
        // 00507109 | D9 81 EC 00 00 00 | fld dword ptr [ecx+EC]
        // 0050710F | D8 0D 18 E8 5D 00 | fmul dword ptr [005DE818]
        // 00507115 | D9 54 24 08       | fst dword ptr [esp+8]

        {"\x89\xBE\xF8\x00\x00\x00\x89\xBE\xF8\x00\x00\x00\x89\xBE\xFC\x00\x00\x00", 18},
        // DISASSEMBLED CODE - PATTERN 4 (UNMODIFIED)
        // 0050763A | 89 BE F8 00 00 00 | mov dword ptr [esi+F8],edi
        // 00507640 | D8 0D 18 E8 5D 00 | fmul dword ptr [005DE818]
        // 00507646 | 89 BE FC 00 00 00 | mov dword ptr [esi+FC],edi

        {"\xC7\x45\xD0\x00\x00\x00\x00\xD8\x0D\x18\xE8\x5D\x00\xC7\x45\xD4\x00\x00\x00\x00", 20},
        // DISASSEMBLED CODE - PATTERN 5 (UNMODIFIED)
        // 0053B8DB | C7 45 D0 00 00 00 00 | mov dword ptr [ebp-30],0
        // 0053B8E2 | D8 0D 18 E8 5D 00    | fmul dword ptr [005DE818]
        // 0053B8E8 | C7 45 D4 00 00 00 00 | mov dword ptr [ebp-2C],0

        {"\xD9\x40\x04\xD8\x0D\x18\xE8\x5D\x00\x8B\x45\x08", 12}
        // DISASSEMBLED CODE - PATTERN 6 (UNMODIFIED)
        // 0053B933 | D9 40 04          | fld dword ptr [eax+4]
        // 0053B936 | D8 0D 18 E8 5D 00 | fmul dword ptr [005DE818]
        // 0053B93C | 8B 45 08          | mov eax,dword ptr [ebp+8]
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xD8\x0D\x20\xDA\x5D\x00\xD9\x90\xF0\x00\x00\x00", 12},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 00507291 | D8 0D 20 DA 5D 00 | fmul dword ptr [005DDA20]
        // 00507297 | D9 90 F0 00 00 00 | fst dword ptr [eax+F0]

        {"\xC7\x81\x38\x01\x00\x00\x01\x00\x00\x00\xD8\x0D\x20\xDA\x5D\x00\xC7\x81\xF8\x00\x00\x00\x01\x00\x00\x00", 26},
        // DISASSEMBLED CODE - PATTERN 2 (MODIFIED)
        // 005070E7 | C7 81 38 01 00 00 01 00 00 00 | mov dword ptr [ecx+138],1
        // 005070F1 | D8 0D 20 DA 5D 00             | fmul dword ptr [005DDA20]
        // 005070F7 | C7 81 F8 00 00 00 01 00 00 00 | mov dword ptr [ecx+F8],1

        {"\xD9\x81\xEC\x00\x00\x00\xD9\x81\xEC\x00\x00\x00\xD9\x54\x24\x08", 16},
        // DISASSEMBLED CODE - PATTERN 3 (MODIFIED)
        // 00507109 | D9 81 EC 00 00 00 | fld dword ptr [ecx+EC]
        // 0050710F | D8 0D 20 DA 5D 00 | fmul dword ptr [005DDA20]
        // 00507115 | D9 54 24 08       | fst dword ptr [esp+8]

        {"\x89\xBE\xF8\x00\x00\x00\xD8\x0D\x20\xDA\x5D\x00\x89\xBE\xFC\x00\x00\x00", 18},
        // DISASSEMBLED CODE - PATTERN 4 (MODIFIED)
        // 0050763A | 89 BE F8 00 00 00 | mov dword ptr [esi+F8],edi
        // 00507640 | D8 0D 20 DA 5D 00 | fmul dword ptr [005DDA20]
        // 00507646 | 89 BE FC 00 00 00 | mov dword ptr [esi+FC],edi

        {"\xC7\x45\xD0\x00\x00\x00\x00\xD8\x0D\x20\xDA\x5D\x00\xC7\x45\xD4\x00\x00\x00\x00", 20},
        // DISASSEMBLED CODE - PATTERN 5 (MODIFIED)
        // 0053B8DB | C7 45 D0 00 00 00 00 | mov dword ptr [ebp-30],0
        // 0053B8E2 | D8 0D 20 DA 5D 00    | fmul dword ptr [005DDA20]
        // 0053B8E8 | C7 45 D4 00 00 00 00 | mov dword ptr [ebp-2C],0

        {"\xD9\x40\x04\xD8\x0D\x20\xDA\x5D\x00\x8B\x45\x08", 12}
        // DISASSEMBLED CODE - PATTERN 6 (MODIFIED)
        // 0053B933 | D9 40 04          | fld dword ptr [eax+4]
        // 0053B936 | D8 0D 20 DA 5D 00 | fmul dword ptr [005DDA20]
        // 0053B93C | 8B 45 08          | mov eax,dword ptr [ebp+8]
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

double NewCameraHorizontalFOVCalculation(double &oldHorizontalCameraFOVValue, uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newAspectRatio = static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue);
    tanValue = tan(DegToRad(oldHorizontalCameraFOVValue / 2.0));
    newCameraHorizontalFOVValue = (2.0 * RadToDeg(atan(newAspectRatio / oldAspectRatio * tanValue))) / 160.0;

    return newCameraHorizontalFOVValue;
}

int main()
{

    cout << "MechWarrior 3: Pirate's Moon (1999) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        OpenFile(file, "mech3.exe");

        SearchAndReplacePatterns(file);

        newCameraHorizontalFOV = static_cast<float>(NewCameraHorizontalFOVCalculation(oldCameraHorizontalFOV = 80.0, newWidth, newHeight));

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