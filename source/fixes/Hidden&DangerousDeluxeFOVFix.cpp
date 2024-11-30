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
const streampos kCameraHFOVOffset = 0x000E6337;
const streampos kCameraFOVOffset = 0x000E633B;
const streampos kObjectRenderingSidesOffset = 0x000E633F;
const streampos kRenderingDistanceOffset = 0x000E6326;

// Variables
uint32_t newWidth, newHeight;
string input;
fstream file;
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
float optimalObjectRenderingSidesValue = 200.0f, newObjectRenderingSidesValue, newRenderingDistance, defaultCameraFOV = 0.5f, defaultRenderingDistance = 0.5f;
double originalAspectRatio = 4.0 / 3.0; // 4:3
float newCameraHFOV, newCameraFOV;
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

float HandleMultiplierInput()
{
    float customMultiplier;

    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
        customMultiplier = stof(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (customMultiplier <= 0)
        {
            cout << "Please enter a number greater than 0 for the FOV." << endl;
        }
    } while (customMultiplier <= 0);

    return customMultiplier;
}

// Function to handle user input in resolution
uint32_t HandleResolutionInput()
{
    uint32_t newCustomResolutionValue;
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

    return newCustomResolutionValue;
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
        {"\xD9\x5D\xF8\xD9\x45\xFC\xD8\xF1", 8},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 10001378 | D9 5D F8 | fstp dword ptr [ebp-8]
        // 1000137B | D9 45 FC | fld dword ptr [ebp-4]
        // 1000137E | D8 F1    | fdiv st(0),st(1)

        {"\xD9\x83\xE4\x01\x00\x00\xD8\x0D\x9C\x72\x0E\x10\xD9\xC0", 14},
        // DISASSEMBLED CODE - PATTERN 2 (UNMODIFIED)
        // 10001351 | D9 83 E4 01 00 00 | fld dword ptr [ebx+000001E4]
        // 10001357 | D8 0D 9C 72 0E 10 | fmul dword ptr [100E729C]
        // 1000135D | D9 C0             | fld st(0)

        {"\xD9\x80\xE4\x01\x00\x00\xD8\x0D\x9C\x72\x0E\x10\x89\x8D\x7C\xFF\xFF\xFF", 18},
        // DISASSEMBLED CODE - PATTERN 3 (UNMODIFIED)
        // 1009D7D5 | D9 80 E4 01 00 00 | fld dword ptr [eax+000001E4]
        // 1009D7DB | D8 0D 9C 72 0E 10 | fmul dword ptr [100E729C]
        // 1009D7E1 | 89 8D 7C FF FF FF | mov [ebp-84],ecx

        {"\xD9\xFE\xD9\x83\xEC\x01\x00\x00\xD8\xA3\xE8\x01\x00\x00", 14},
        // DISASSEMBLED CODE - PATTERN 4 (UNMODIFIED)
        // 10001364 | D9 FE             | fsin
        // 10001366 | D9 83 EC 01 00 00 | fld dword ptr [ebx+000001EC]
        // 1000136C | D8 A3 E8 01 00 00 | fsub dword ptr [ebx+000001E8]

        {"\xE9\xBF\x22\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 60}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xD9\x5D\xF8\xE9\x8E\x4F\x0E\x00", 8},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 10001378 | D9 5D F8       | fstp dword ptr [ebp-8]
        // 1000137B | E9 8E 4F 0E 00 | jmp 100E630E

        {"\xD9\x83\xE4\x01\x00\x00\xD8\x0D\x3B\x63\x0E\x10\xD9\xC0", 14},
        // DISASSEMBLED CODE - PATTERN 2 (MODIFIED)
        // 10001351 | D9 83 E4 01 00 00 | fld dword ptr [ebx+000001E4]
        // 10001357 | D8 0D 3B 63 0E 10 | fmul dword ptr [100E633B]
        // 1000135D | D9 C0             | fld st(0)

        {"\xD9\x80\xE4\x01\x00\x00\xD8\x0D\x3F\x63\x0E\x10\x89\x8D\x7C\xFF\xFF\xFF", 18},
        // DISASSEMBLED CODE - PATTERN 3 (MODIFIED)
        // 1009D7D5 | D9 80 E4 01 00 00 | fld dword ptr [eax+000001E4]
        // 1009D7DB | D8 0D 3F 63 0E 10 | fmul dword ptr [100E633F]
        // 1009D7E1 | 89 8D 7C FF FF FF | mov [ebp-84],ecx

        {"\xD9\xFE\xE9\xB5\x4F\x0E\x00\x90\xD8\xA3\xE8\x01\x00\x00", 14},
        // DISASSEMBLED CODE - PATTERN 4 (MODIFIED)
        // 10001364 | D9 FE             | fsin
        // 10001366 | E9 B5 4F 0E 00    | jmp 100E6320
        // 1000136B | 90                | nop
        // 1000136C | D8 A3 E8 01 00 00 | fsub dword ptr [ebx+000001E8]

        {"\xE9\xBF\x22\xFF\xFF\x00\x00\xD9\x45\xFC\xD8\x0D\x37\x63\x0F\x10\xD8\xF1\xE9\x62\xB0\xF1\xFF\x00\x00\xC7\x83\xEC\x01\x00\x00\x00\x00\x7A\x44\xD9\x83\xEC\x01\x00\x00\xE9\x37\xB0\xF1\xFF\x00\x00\x00\x00\x80\x3F\x00\x00\x00\x3F\x00\x00\x48\x43", 60}
        // DISASSEMBLED CODE - PART OF PATTERN 5 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 100E630E (x32dbg)
        // 100E630E | D9 45 FC                      | fld dword ptr [ebp-4]
        // 100E6311 | D8 0D 37 63 0F 10             | fmul dword ptr [100F6337]
        // 100E6317 | D8 F1                         | fdiv st(0),st(1)
        // 100E6319 | E9 62 B0 F1 FF                | jmp 10001380
        // 100E631E | 00 00                         | add byte ptr [eax],al
        // 100E6320 | C7 83 EC 01 00 00 00 00 7A 44 | mov dword ptr [ebx+000001EC],447A0000
        // 100E632A | D9 83 EC 01 00 00             | fld dword ptr [ebx+000001EC]
        // 100E6330 | E9 37 B0 F1 FF                | jmp 1000136C
        // 100E6335 | 00 00                         | add byte ptr [eax],al
        // 100E6337 | 00 00                         | add byte ptr [eax],al
        // 100E6339 | 80 3F 00                      | cmp byte ptr [edi],0
        // 100E633C | 00 00                         | add byte ptr [eax],al
        // 100E633E | 3F                            | aas
        // 100E633F | 0000                          | add byte ptr [eax],al
        // 100E6341 | 48                            | dec eax
        // 100E6342 | 43                            | inc ebx
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

double CameraHFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    return (originalAspectRatio) / (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue));
}

int main()
{

    cout << "Hidden & Dangerous Deluxe (2003) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newCameraHFOV = static_cast<float>(CameraHFOVCalculation(newWidth, newHeight));

        cout << "- Do you wish to set a custom camera FOV, rendering on the sides and rendering distance values (1) or leave them as default values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "- Type the desired camera FOV multiplier (default for 4:3 is 0.5): ";
            newCameraFOV = HandleMultiplierInput();

            cout << "- Type the desired rendering distance multiplier (default for 4:3 is 0.5): ";
            newRenderingDistance = HandleMultiplierInput();

            cout << "- Type the desired rendering distance on the sides multiplier (default for 4:3 is 0.5, a very high value is recommended to avoid engine culling but might cause lag in older PCs): ";
            newObjectRenderingSidesValue = HandleMultiplierInput();

            break;

        case 2:
            newCameraFOV = defaultCameraFOV;
            newRenderingDistance = defaultRenderingDistance;
            newObjectRenderingSidesValue = optimalObjectRenderingSidesValue;
            break;
        }

        OpenFile(file, "i3d2.dll");

        SearchAndReplacePatterns(file);

        file.seekp(kCameraHFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHFOV), sizeof(newCameraHFOV));

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        file.seekp(kObjectRenderingSidesOffset);
        file.write(reinterpret_cast<const char *>(&newObjectRenderingSidesValue), sizeof(newObjectRenderingSidesValue));

        file.seekp(kRenderingDistanceOffset);
        file.write(reinterpret_cast<const char *>(&newRenderingDistance), sizeof(newRenderingDistance));

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
        HandleChoiceInput(choice2);

        if (choice2 == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}