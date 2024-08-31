#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>
#include <vector>
#include <cstring>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const float kPi = 3.14159265358979323846f;
const double kTolerance = 0.01;
const streampos kCameraHorizontalFOVOffset = 0x001427C3;
const streampos kCameraVerticalFOVOffset = 0x001427D5;
const streampos kHUD_and_Character_Selection_AspectRatio_Offset = 0x000AD771;
const streampos kHUD_and_Character_Selection_FOV_Offset = 0x000AD776;
const streampos kAspectRatio2Offset = 0x000AD4A6;
const streampos kFOV2Offset = 0x000AD4AB;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight, newCustomResolutionValue;
string input;
fstream file;
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
float customFOV, newAspectRatio, newGameplayCameraHorizontalFOV, newGameplayCameraVerticalFOV, newHUDAndCharacterSelectionFOV, newFOV2;
char ch;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
    return degrees * (kPi / 180.0);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
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

float HandleFOVInput()
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

    return customFOV;
}

// Function to handle user input in resolution
uint32_t HandleResolutionInput()
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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << filename << " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

void SearchAndReplacePatterns(fstream &file)
{
    // Defines the original and new patterns with their sizes
    vector<pair<const char *, size_t>> patterns = {

        {"\xD9\x40\x3C\xD8\x49\x28\xD9\x5D\xF8\x8B\x55\x08\x8B\x45\x08\xD9\x42\x40\xD8\x48\x28", 21},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 00436A74 | D940 3C | fld dword ptr [eax+3C]
        // 00436A77 | D849 28 | fmul dword ptr [ecx+28]
        // 00436A7A | D95D F8 | fstp dword ptr [ebp-8]
        // 00436A7D | 8B55 08 | mov edx,[ebp+8]
        // 00436A80 | 8B45 08 | mov eax,[ebp+8]
        // 00436A83 | D942 40 | fld dword ptr [edx+40]
        // 00436A86 | D848 28 | fmul dword ptr [eax+28]
        {"\x14\x51\xE8\x59\x85\xFB\xFF\x83\xC4\x08\x5E\x33\xC0\x5B\x83\xC4\x2C\xC3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 84}};

    vector<pair<const char *, size_t>> replacements = {

        {"\xE9\x47\xBD\x10\x00\x90\xD9\x5D\xF8\x8B\x55\x08\x8B\x45\x08\xE9\x4A\xBD\x10\x00\x90", 21},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 00436A74 | E9 47BD1000 | jmp 005427C0
        // 00436A79 | 90          | nop
        // 00436A7A | D95D F8     | fstp dword ptr [ebp-8]
        // 00436A7D | 8B55 08     | mov edx,[ebp+8]
        // 00436A80 | 8B45 08     | mov eax,[ebp+8]
        // 00436A83 | E9 4ABD1000 | jmp 005427D2
        // 00436A88 | 90          | nop
        {"\x14\x51\xE8\x59\x85\xFB\xFF\x83\xC4\x08\x5E\x33\xC0\x5B\x83\xC4\x2C\xC3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x40\x3C\x00\x00\x00\x40\xD9\x40\x3C\xD8\x49\x28\xE9\xA8\x42\xEF\xFF\xC7\x42\x40\x00\x00\x00\x40\xD9\x42\x40\xD8\x48\x28\xE9\xA5\x42\xEF\xFF", 84}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 005427C0 (x32dbg)
        // 005427C0 | C740 3C 00000040 | mov [eax+3C],40000000
        // 005427C7 | D940 3C          | fld dword ptr [eax+3C]
        // 005427CA | D849 28          | fmul dword ptr [ecx+28]
        // 005427CD | E9 A842EFFF      | jmp 00436A7A
        // 005427D2 | C742 40 00000040 | mov dword ptr [edx+40],40000000
        // 005427D9 | D942 40          | fld dword ptr [edx+40]
        // 005427DC | D848 28          | fmul dword ptr [eax+28]
        // 005427DF | E9 A542EFFF      | jmp 00436A89
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
    cout << "Antz Extreme Racing (2002) Widescreen Fixer v1.1 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\n- Enter the desired height: ";
        newHeight = HandleResolutionInput();

        newAspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);

        newHUDAndCharacterSelectionFOV = 0.5f * ((static_cast<float>(newWidth) / static_cast<float>(newHeight)) / (4.0f / 3.0f));

        cout << "\n- Do you want to fix the FOV automatically based on the resolution typed above (1) or set custom multiplier values for horizontal and vertical FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            newGameplayCameraHorizontalFOV = 0.5f * ((static_cast<float>(newWidth) / static_cast<float>(newHeight)) / (4.0f / 3.0f));

            newGameplayCameraVerticalFOV = 0.375f;

            break;

        case 2:
            cout << "\n- Type a custom horizontal FOV multiplier value (default for 4:3 aspect ratio is 0.5): ";
            newGameplayCameraHorizontalFOV = HandleFOVInput();

            cout << "\n- Type a custom vertical FOV multiplier value (default for 4:3 aspect ratio is 0.375): ";
            newGameplayCameraVerticalFOV = HandleFOVInput();
            break;
        }

        OpenFile(file, "antzextremeracing.exe");

        SearchAndReplacePatterns(file);

        file.seekp(kCameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newGameplayCameraHorizontalFOV), sizeof(newGameplayCameraHorizontalFOV));

        file.seekp(kCameraVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newGameplayCameraVerticalFOV), sizeof(newGameplayCameraVerticalFOV));

        file.seekp(kHUD_and_Character_Selection_AspectRatio_Offset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kHUD_and_Character_Selection_FOV_Offset);
        file.write(reinterpret_cast<const char *>(&newHUDAndCharacterSelectionFOV), sizeof(newHUDAndCharacterSelectionFOV));

        file.seekp(kAspectRatio2Offset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kFOV2Offset);
        file.write(reinterpret_cast<const char *>(&newFOV2), sizeof(newFOV2));

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
            cout << "\nPress Enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}