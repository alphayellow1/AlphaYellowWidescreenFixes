#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type // For unin32_t variable type
#include <cmath>
#include <limits>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>
#include <windows.h>
#include <vector>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const double kTolerance = 0.01;
const double kDefaultCameraVerticalFOVInRadiansGameplay = 1.178097248;
const double kDefaultCameraVerticalFOVInRadiansCutscene1 = 0.4188790917;
const double kDefaultCameraVerticalFOVInRadiansCutscene2 = 0.3665191829;
const streampos kGameplayCameraHorizontalFOVOffset = 0x000B8E1C;
const streampos kGameplayCameraVerticalFOVOffset = 0x000B8E46;
const streampos kCutscene1CameraHorizontalFOVOffset = 0x000B8E2A;
const streampos kCutscene1CameraVerticalFOVOffset = 0x000B8E54;
const streampos kCutscene2CameraHorizontalFOVOffset = 0x000B8E38;
const streampos kCutscene2CameraVerticalFOVOffset = 0x000B8E62;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float currentCameraHorizontalFOVInRadians, currentCameraVerticalFOVInRadians;
double currentCameraHorizontalFOVInDegrees, currentCameraVerticalFOVInDegrees,
    newCameraHorizontalFOVInRadiansGameplay, newCameraVerticalFOVInRadiansGameplay,
    newCameraHorizontalFOVInRadiansCutscene1, newCameraVerticalFOVInRadiansCutscene1,
    newCameraHorizontalFOVInRadiansCutscene2, newCameraVerticalFOVInRadiansCutscene2,
    newCameraHorizontalFOVInDegreesGameplay, newCameraVerticalFOVInDegreesGameplay,
    newCameraHorizontalFOVInDegreesCutscene1, newCameraVerticalFOVInDegreesCutscene1,
    newCameraHorizontalFOVInDegreesCutscene2, newCameraVerticalFOVInDegreesCutscene2,
    oldWidth = 4.0, oldHeight = 3.0, oldAspectRatio = oldWidth / oldHeight;
string descriptor1, descriptor2, input;
fstream file;
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

double HandleFOVInput()
{
    double newCustomFOVInDegrees;

    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
        newCustomFOVInDegrees = stod(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newCustomFOVInDegrees <= 0 || newCustomFOVInDegrees >= 180)
        {
            cout << "Please enter a valid number for the FOV (greater than 0 and less than 180)." << endl;
        }
    } while (newCustomFOVInDegrees <= 0 || newCustomFOVInDegrees >= 180);

    return newCustomFOVInDegrees;
}

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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
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
        {"\x8B\x81\x98\x01\x00\x00\x89\x45\xBC\x8B\x81\x9C\x01\x00\x00", 15},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 0040E35A | 8B 81 98 01 00 00 | mov eax,dword ptr ds:[ecx+198]
        // 0040E360 | 89 45 BC          | mov dword ptr ss:[ebp-44],eax
        // 0040E363 | 8B 81 9C 01 00 00 | mov eax,dword ptr ds:[ecx+19C]

        {"\xE9\x50\x73\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 189}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\x55\xAA\x0A\x00\x90\x89\x45\xBC\xE9\x7D\xAA\x0A\x00\x90", 15},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 0040E35A | E9 55 AA 0A 00 | jmp 004B8DB4
        // 0040E35F | 90             | nop
        // 0040E360 | 89 45 BC       | mov dword ptr ss:[ebp-44],eax
        // 0040E363 | E9 7D AA 0A 00 | jmp 004B8DE5
        // 0040E368 | 90             | nop

        {"\xE9\x50\x73\xFF\xFF\x00\x00\x00\x00\x81\xB9\x98\x01\x00\x00\xDB\x0F\xC9\x3F\x74\x56\x81\xB9\x98\x01\x00\x00\x36\xFA\x0E\x3F\x74\x58\x81\xB9\x98\x01\x00\x00\xDE\x35\xFA\x3E\x74\x5A\x8B\x81\x98\x01\x00\x00\xE9\x7D\x55\xF5\xFF\x00\x00\x81\xB9\x9C\x01\x00\x00\xE4\xCB\x96\x3F\x74\x4F\x81\xB9\x9C\x01\x00\x00\x52\x77\xD6\x3E\x74\x51\x81\xB9\x9C\x01\x00\x00\x67\xA8\xBB\x3E\x74\x53\x8B\x81\x9C\x01\x00\x00\xE9\x55\x55\xF5\xFF\x00\x00\xC7\x81\x98\x01\x00\x00\x1A\x63\xED\x3F\xEB\x9E\x00\x00\xC7\x81\x98\x01\x00\x00\xD3\xFC\x3A\x3F\xEB\x9C\x00\x00\xC7\x81\x98\x01\x00\x00\xA9\x55\x24\x3F\xEB\x9A\x00\x00\xC7\x81\x9C\x01\x00\x00\xE4\xCB\x96\x3F\xEB\xA5\x00\x00\xC7\x81\x9C\x01\x00\x00\x52\x77\xD6\x3E\xEB\xA3\x00\x00\xC7\x81\x9C\x01\x00\x00\x67\xA8\xBB\x3E\xEB\xA1", 189}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 004B8DB4 (x32dbg)
        // 004B8DB4 | 81 B9 98 01 00 00 DB 0F C9 3F | cmp dword ptr ds:[ecx+198],3FC90FDB
        // 004B8DBE | 74 56                         | je 004B8E16
        // 004B8DC0 | 81 B9 98 01 00 00 36 FA 0E 3F | cmp dword ptr ds:[ecx+198],3F0EFA36
        // 004B8DCA | 74 58                         | je 004B8E24
        // 004B8DCC | 81 B9 98 01 00 00 DE 35 FA 3E | cmp dword ptr ds:[ecx+198],3EFA35DE
        // 004B8DD6 | 74 5A                         | je 004B8E32
        // 004B8DD8 | 8B 81 98 01 00 00             | mov eax,dword ptr ds:[ecx+198]
        // 004B8DDE | E9 7D 55 F5 FF                | jmp 0040E360
        // 004B8DE3 | 00 00                         | add byte ptr ds:[eax],al
        // 004B8DE5 | 81 B9 9C 01 00 00 E4 CB 96 3F | cmp dword ptr ds:[ecx+19C],3F96CBE4
        // 004B8DEF | 74 4F                         | je 004B8E40
        // 004B8DF1 | 81 B9 9C 01 00 00 52 77 D6 3E | cmp dword ptr ds:[ecx+19C],3ED67752
        // 004B8DFB | 74 51                         | je 004B8E4E
        // 004B8DFD | 81 B9 9C 01 00 00 67 A8 BB 3E | cmp dword ptr ds:[ecx+19C],3EBBA867
        // 004B8E07 | 74 53                         | je 004B8E5C
        // 004B8E09 | 8B 81 9C 01 00 00             | mov eax,dword ptr ds:[ecx+19C]
        // 004B8E0F | E9 55 55 F5 FF                | jmp 0040E369
        // 004B8E14 | 00 00                         | add byte ptr ds:[eax],al
        // 004B8E16 | C7 81 98 01 00 00 1A 63 ED 3F | mov dword ptr ds:[ecx+198],3FED631A
        // 004B8E20 | EB 9E                         | jmp 004B8DC0
        // 004B8E22 | 00 00                         | add byte ptr ds:[eax],al
        // 004B8E24 | C7 81 98 01 00 00 D3 FC 3A 3F | mov dword ptr ds:[ecx+198],3F3AFCD3
        // 004B8E2E | EB 9C                         | jmp 004B8DCC
        // 004B8E30 | 00 00                         | add byte ptr ds:[eax],al
        // 004B8E32 | C7 81 98 01 00 00 A9 55 24 3F | mov dword ptr ds:[ecx+198],3F2455A9
        // 004B8E3C | EB 9A                         | jmp 004B8DD8
        // 004B8E3E | 00 00                         | add byte ptr ds:[eax],al
        // 004B8E40 | C7 81 9C 01 00 00 E4 CB 96 3F | mov dword ptr ds:[ecx+19C],3F96CBE4
        // 004B8E4A | EB A5                         | jmp 004B8DF1
        // 004B8E4C | 00 00                         | add byte ptr ds:[eax],al
        // 004B8E4E | C7 81 9C 01 00 00 52 77 D6 3E | mov dword ptr ds:[ecx+19C],3ED67752
        // 004B8E58 | EB A3                         | jmp 004B8DFD
        // 004B8E5A | 00 00                         | add byte ptr ds:[eax],al
        // 004B8E5C | C7 81 9C 01 00 00 67 A8 BB 3E | mov dword ptr ds:[ecx+19C],3EBBA867
        // 004B8E66 | EB A1                         | jmp 004B8E09
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

double HorizontalCameraFOVInDegreesCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue, double oldHorizontalFOV)
{
    return 2.0 * RadToDeg(atan(tan(DegToRad(oldHorizontalFOV / 2.0)) * (static_cast<double>(newWidthValue) / newHeightValue) / oldAspectRatio));
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Die Hard: Nakatomi Plaza (2002) FOV Fixer v1.1 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "Lithtech.exe");

        SearchAndReplacePatterns(file);

        file.seekg(kGameplayCameraHorizontalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraHorizontalFOVInRadians), sizeof(currentCameraHorizontalFOVInRadians));

        file.seekg(kGameplayCameraVerticalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraVerticalFOVInRadians), sizeof(currentCameraVerticalFOVInRadians));

        // Converts the FOV values from radians to degrees
        currentCameraHorizontalFOVInDegrees = RadToDeg((double)currentCameraHorizontalFOVInRadians);
        currentCameraVerticalFOVInDegrees = RadToDeg((double)currentCameraVerticalFOVInRadians);

        cout << "\nCurrent FOV: " << currentCameraHorizontalFOVInDegrees << "\u00B0 (Horizontal); " << currentCameraVerticalFOVInDegrees << "\u00B0 (Vertical)" << endl;

        cout << "\n- Enter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\n- Enter the desired height: ";
        newHeight = HandleResolutionInput();

        newCameraHorizontalFOVInDegreesCutscene1 = HorizontalCameraFOVInDegreesCalculation(newWidth, newHeight, 32.000003);

        newCameraHorizontalFOVInDegreesCutscene2 = HorizontalCameraFOVInDegreesCalculation(newWidth, newHeight, 28.000002);

        newCameraHorizontalFOVInRadiansCutscene1 = DegToRad(newCameraHorizontalFOVInDegreesCutscene1);

        newCameraHorizontalFOVInRadiansCutscene2 = DegToRad(newCameraHorizontalFOVInDegreesCutscene2);

        newCameraVerticalFOVInRadiansCutscene1 = kDefaultCameraVerticalFOVInRadiansCutscene1;

        newCameraVerticalFOVInRadiansCutscene2 = kDefaultCameraVerticalFOVInRadiansCutscene2;

        cout << "\n- Do you want to set FOV automatically based on the typed resolution above (1) or set custom horizontal and vertical FOV values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            newCameraHorizontalFOVInDegreesGameplay = HorizontalCameraFOVInDegreesCalculation(newWidth, newHeight, 90.0);

            newCameraHorizontalFOVInRadiansGameplay = DegToRad(newCameraHorizontalFOVInDegreesGameplay);

            newCameraVerticalFOVInRadiansGameplay = kDefaultCameraVerticalFOVInRadiansGameplay;

            newCameraVerticalFOVInDegreesGameplay = RadToDeg(newCameraVerticalFOVInRadiansGameplay);

            descriptor1 = "fixed";

            descriptor2 = ".";

            break;

        case 2:
            cout << "\n- Enter the desired horizontal FOV (in degrees, default for 4:3 is 90\u00B0): ";
            newCameraHorizontalFOVInDegreesGameplay = HandleFOVInput();

            cout << "\n- Enter the desired vertical FOV (in degrees, default for 4:3 is 67.5\u00B0): ";
            newCameraVerticalFOVInDegreesGameplay = HandleFOVInput();

            newCameraHorizontalFOVInRadiansGameplay = DegToRad(newCameraHorizontalFOVInDegreesGameplay); // Converts degrees to radians

            newCameraVerticalFOVInRadiansGameplay = DegToRad(newCameraVerticalFOVInDegreesGameplay); // Converts degrees to radians

            descriptor1 = "manually";

            descriptor2 = " to " + to_string(newCameraHorizontalFOVInDegreesGameplay) + "\u00B0 and vertical FOV to " + to_string(newCameraVerticalFOVInDegreesGameplay) + "\u00B0.";

            break;
        }

        float newCameraHorizontalFOVInRadiansGameplayAsFloat = static_cast<float>(newCameraHorizontalFOVInRadiansGameplay);

        float newCameraVerticalFOVInRadiansGameplayAsFloat = static_cast<float>(newCameraVerticalFOVInRadiansGameplay);

        float newCameraHorizontalFOVInRadiansCutscene1AsFloat = static_cast<float>(newCameraHorizontalFOVInRadiansCutscene1);

        float newCameraVerticalFOVInRadiansCutscene1AsFloat = static_cast<float>(newCameraVerticalFOVInRadiansCutscene1);

        float newCameraHorizontalFOVInRadiansCutscene2AsFloat = static_cast<float>(newCameraHorizontalFOVInRadiansCutscene2);

        float newCameraVerticalFOVInRadiansCutscene2AsFloat = static_cast<float>(newCameraVerticalFOVInRadiansCutscene2);

        file.seekp(kGameplayCameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOVInRadiansGameplayAsFloat), sizeof(newCameraHorizontalFOVInRadiansGameplayAsFloat));

        file.seekp(kGameplayCameraVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraVerticalFOVInRadiansGameplayAsFloat), sizeof(newCameraVerticalFOVInRadiansGameplayAsFloat));

        file.seekp(kCutscene1CameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOVInRadiansCutscene1AsFloat), sizeof(newCameraHorizontalFOVInRadiansCutscene1AsFloat));

        file.seekp(kCutscene1CameraVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraVerticalFOVInRadiansCutscene1AsFloat), sizeof(newCameraVerticalFOVInRadiansCutscene1AsFloat));

        file.seekp(kCutscene2CameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOVInRadiansCutscene2AsFloat), sizeof(newCameraHorizontalFOVInRadiansCutscene2AsFloat));

        file.seekp(kCutscene2CameraVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraVerticalFOVInRadiansCutscene2AsFloat), sizeof(newCameraVerticalFOVInRadiansCutscene2AsFloat));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully " << descriptor1 << " the field of view" << descriptor2 << endl;
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

        cout << "\n-----------------------------------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}