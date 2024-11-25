#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type
#include <cmath>
#include <limits>
#include <windows.h>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>
#include <vector>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const double kTolerance = 0.01;
const double kHipfireAndCutscenesDefaultCameraVFOVInRadians = 0.87266469;
const double kZoomedInDefaultCameraVFOVInRadians = 0.5817760229;
const streampos kHipfireAndCutscenesCameraHFOVOffset = 0x000CC428;
const streampos kHipfire2AndCutscenesCameraHFOVOffset = 0x000CC434;
const streampos kHipfireAndCutscenesCameraVFOVOffset = 0x000CC440;
const streampos kZoomedInCameraHFOVOffset = 0x000CC447;
const streampos kZoomedInCameraVFOVOffset = 0x000CC44E;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float currentHipfireAndCutscenesCameraHFOVInRadians, currentHipfireAndCutscenesCameraVFOVInRadians, currentZoomedInCameraHFOVInRadians, currentZoomedInCameraVFOVInRadians, 
newHipfireAndCutscenesCameraHFOVInRadiansAsFloat, newHipfireAndCutscenesCameraVFOVInRadiansAsFloat, newZoomedInCameraHFOVInRadiansAsFloat, newZoomedInCameraVFOVInRadiansAsFloat;
double currentHipfireAndCutscenesCameraHFOVInDegrees, currentHipfireAndCutscenesCameraVFOVInDegrees, currentZoomedInCameraHFOVInDegrees, currentZoomedInCameraVFOVInDegrees, 
newHipfireAndCutscenesCameraHFOVInRadians, newHipfireAndCutscenesCameraVFOVInRadians, newHipfireAndCutscenesCameraHFOVInDegrees, 
newHipfireAndCutscenesCameraVFOVInDegrees, newZoomedInCameraHFOVInDegrees, newZoomedInCameraVFOVInDegrees, newZoomedInCameraHFOVInRadians, newZoomedInCameraVFOVInRadians, oldWidth = 4.0, oldHeight = 3.0, oldAspectRatio = oldWidth / oldHeight;
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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it isn't currently running. Press Enter when all the mentioned problems are solved." << endl;
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
        // 0040E4F3 | 8B 81 98 01 00 00 | mov eax,dword ptr ds:[ecx+198]
        // 0040E4F9 | 89 45 BC          | mov dword ptr ss:[ebp-44],eax
        // 0040E4FC | 8B 81 9C 01 00 00 | mov eax,dword ptr ds:[ecx+19C]

        {"\x89\x81\x98\x01\x00\x00\x8B\x45\x10\x89\x81\x9C\x01\x00\x00", 15},
        // DISASSEMBLED CODE - PATTERN 2 (UNMODIFIED)
        // 004088D7 | 89 81 98 01 00 00 | mov dword ptr ds:[ecx+198],eax
        // 004088DD | 8B 45 10          | mov eax,dword ptr ss:[ebp+10]
        // 004088E0 | 89 81 9C 01 00 00 | mov dword ptr ds:[ecx+19C],eax

        {"\xE9\xEA\x3B\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 157}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\xC8\xDE\x0B\x00\x90\x89\x45\xBC\xE9\xE2\xDE\x0B\x00\x90", 15},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 0040E4F3 | E9 C8 DE 0B 00 | jmp 004CC3C0
        // 0040E4F8 | 90             | nop
        // 0040E4F9 | 89 45 BC       | mov dword ptr ss:[ebp-44],eax
        // 0040E4FC | E9 E2 DE 0B 00 | jmp 004CC3E3
        // 0040E501 | 90             | nop

        {"\xE9\x1E\x3B\x0C\x00\x90\x8B\x45\x10\xE9\x27\x3B\x0C\x00\x90", 15},
        // DISASSEMBLED CODE - PATTERN 2 (MODIFIED)
        // 004088D7 | E9 1E 3B 0C 00 | jmp 004CC3FA
        // 004088DC | 90             | nop
        // 004088DD | 8B 45 10       | mov eax,dword ptr ss:[ebp+10]
        // 004088E0 | E9 27 3B 0C 00 | jmp 004CC40C
        // 004088E5 | 90             | nop

        {"\xE9\xEA\x3B\xFF\xFF\x00\x00\x00\x00\x81\xB9\x98\x01\x00\x00\x92\x0A\x86\x3F\x74\x56\x81\xB9\x98\x01\x00\x00\xA6\x0A\x86\x3F\x74\x56\x8B\x81\x98\x01\x00\x00\xE9\x16\x21\xF4\xFF\x81\xB9\x9C\x01\x00\x00\xF4\x66\x5F\x3F\x74\x4B\x8B\x81\x9C\x01\x00\x00\xE9\x08\x21\xF4\xFF\x3D\xD8\xB8\x32\x3F\x74\x45\x89\x81\x98\x01\x00\x00\xE9\xD1\xC4\xF3\xFF\x3D\x46\xEF\x14\x3F\x74\x3A\x89\x81\x9C\x01\x00\x00\xE9\xC8\xC4\xF3\xFF\x00\x00\x00\x00\xC7\x81\x98\x01\x00\x00\x28\xF4\xA7\x3F\xEB\x9E\xC7\x81\x98\x01\x00\x00\x28\xF4\xA7\x3F\xEB\x9E\xC7\x81\x9C\x01\x00\x00\xF4\x66\x5F\x3F\xEB\xA9\xB8\x82\x4F\x67\x3F\xEB\xB4\xB8\x46\xEF\x14\x3F\xEB\xBF", 157}
        // DISASSEMBLED CODE - PART OF PATTERN 3 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 004CC3C0 (x32dbg)
        // 004CC3C0 | 81 B9 98 01 00 00 92 0A 86 3F | cmp dword ptr ds:[ecx+198],3F860A92
        // 004CC3CA | 74 56                         | je 004CC422
        // 004CC3CC | 81 B9 98 01 00 00 A6 0A 86 3F | cmp dword ptr ds:[ecx+198],3F860AA6
        // 004CC3D6 | 74 56                         | je 004CC42E
        // 004CC3D8 | 8B 81 98 01 00 00             | mov eax,dword ptr ds:[ecx+198]
        // 004CC3DE | E9 16 21 F4 FF                | jmp 0040E4F9
        // 004CC3E3 | 81 B9 9C 01 00 00 F4 66 5F 3F | cmp dword ptr ds:[ecx+19C],3F5F66F4
        // 004CC3ED | 74 4B                         | je 004CC43A
        // 004CC3EF | 8B 81 9C 01 00 00             | mov eax,dword ptr ds:[ecx+19C]
        // 004CC3F5 | E9 08 21 F4 FF                | jmp 0040E502
        // 004CC3FA | 3D D8 B8 32 3F                | cmp eax,3F32B8D8
        // 004CC3FF | 74 45                         | je 004CC446
        // 004CC401 | 89 81 98 01 00 00             | mov dword ptr ds:[ecx+198],eax
        // 004CC407 | E9 D1 C4 F3 FF                | jmp 004088DD
        // 004CC40C | 3D 46 EF 14 3F                | cmp eax,3F14EF46
        // 004CC411 | 74 3A                         | je 004CC44D
        // 004CC413 | 89 81 9C 01 00 00             | mov dword ptr ds:[ecx+19C],eax
        // 004CC419 | E9 C8 C4 F3 FF                | jmp 004088E6
        // 004CC41E | 00 00                         | add byte ptr ds:[eax],al
        // 004CC420 | 00 00                         | add byte ptr ds:[eax],al
        // 004CC422 | C7 81 98 01 00 00 28 F4 A7 3F | mov dword ptr ds:[ecx+198],3FA7F428
        // 004CC42C | EB 9E                         | jmp 004CC3CC
        // 004CC42E | C7 81 98 01 00 00 28 F4 A7 3F | mov dword ptr ds:[ecx+198],3FA7F428
        // 004CC438 | EB 9E                         | jmp 004CC3D8
        // 004CC43A | C7 81 9C 01 00 00 F4 66 5F 3F | mov dword ptr ds:[ecx+19C],3F5F66F4
        // 004CC444 | EB A9                         | jmp 004CC3EF
        // 004CC446 | B8 824F673F                   | mov eax,3F674F82
        // 004CC44B | EB B4                         | jmp 004CC401
        // 004CC44D | B8 46 EF 14 3F                | mov eax,3F14EF46
        // 004CC452 | EB BF                         | jmp 004CC413
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

    cout << "Western Outlaw: Wanted Dead or Alive (2003) FOV Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "lithtech.exe");

        file.seekg(kHipfireAndCutscenesCameraHFOVOffset);
        file.read(reinterpret_cast<char *>(&currentHipfireAndCutscenesCameraHFOVInRadians), sizeof(currentHipfireAndCutscenesCameraHFOVInRadians));

        file.seekg(kHipfireAndCutscenesCameraVFOVOffset);
        file.read(reinterpret_cast<char *>(&currentHipfireAndCutscenesCameraVFOVInRadians), sizeof(currentHipfireAndCutscenesCameraVFOVInRadians));

        file.seekg(kZoomedInCameraHFOVOffset);
        file.read(reinterpret_cast<char *>(&currentZoomedInCameraHFOVInRadians), sizeof(currentZoomedInCameraHFOVInRadians));

        file.seekg(kZoomedInCameraVFOVOffset);
        file.read(reinterpret_cast<char *>(&currentZoomedInCameraVFOVInRadians), sizeof(currentZoomedInCameraVFOVInRadians));

        // Converts the FOV values from radians to degrees
        currentHipfireAndCutscenesCameraHFOVInDegrees = RadToDeg((double)currentHipfireAndCutscenesCameraHFOVInRadians);

        currentHipfireAndCutscenesCameraVFOVInDegrees = RadToDeg((double)currentHipfireAndCutscenesCameraVFOVInRadians);

        currentZoomedInCameraHFOVInDegrees = RadToDeg((double)currentZoomedInCameraHFOVInRadians);

        currentZoomedInCameraVFOVInDegrees = RadToDeg((double)currentZoomedInCameraVFOVInRadians);

        cout << "\nCurrent FOVs:" << endl;
        cout << "Hipfire and Cutscenes FOV: " << currentHipfireAndCutscenesCameraHFOVInDegrees << "\u00B0 (Horizontal); " << currentHipfireAndCutscenesCameraVFOVInDegrees << "\u00B0 (Vertical) " << endl;
        cout << "Zoomed-in FOV: " << currentZoomedInCameraHFOVInDegrees << "\u00B0 (Horizontal); " << currentZoomedInCameraVFOVInDegrees << "\u00B0 (Vertical) " << endl;

        cout << "\n- Do you want to set FOV automatically based on the desired resolution (1) or set custom horizontal and vertical FOV values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            newWidth = HandleResolutionInput();

            cout << "\n- Enter the desired height: ";
            newHeight = HandleResolutionInput();

            // Calculates the new horizontal FOV
            newHipfireAndCutscenesCameraHFOVInDegrees = HorizontalCameraFOVInDegreesCalculation(newWidth, newHeight, 60.000002);

            newHipfireAndCutscenesCameraHFOVInRadians = DegToRad(newHipfireAndCutscenesCameraHFOVInDegrees); // Converts degrees to radians

            newHipfireAndCutscenesCameraVFOVInRadians = kHipfireAndCutscenesDefaultCameraVFOVInRadians;

            newHipfireAndCutscenesCameraVFOVInDegrees = RadToDeg(newHipfireAndCutscenesCameraVFOVInRadians);

            newZoomedInCameraHFOVInDegrees = HorizontalCameraFOVInDegreesCalculation(newWidth, newHeight, 40.000074);

            newZoomedInCameraHFOVInRadians = DegToRad(newZoomedInCameraHFOVInDegrees);

            newZoomedInCameraVFOVInRadians = kZoomedInDefaultCameraVFOVInRadians;

            newZoomedInCameraVFOVInDegrees = RadToDeg(newZoomedInCameraVFOVInRadians);

            descriptor1 = "fixed";

            descriptor2 = ".";

            break;

        case 2:
            cout << "\n- Enter the desired horizontal FOV for hipfire and cutscenes (in degrees, default for 4:3 is 60\u00B0): ";
            newHipfireAndCutscenesCameraHFOVInDegrees = HandleFOVInput();

            cout << "\n- Enter the desired vertical FOV for hipfire and cutscenes (in degrees, default for 4:3 is 50\u00B0): ";
            newHipfireAndCutscenesCameraVFOVInDegrees = HandleFOVInput();

            cout << "\n- Enter the desired horizontal FOV for zoomed weapons (in degrees, default for 4:3 is 40\u00B0): ";
            newZoomedInCameraHFOVInDegrees = HandleFOVInput();

            cout << "\n- Enter the desired vertical FOV for zoomed weapons (in degrees, default for 4:3 is 33.3\u00B0): ";
            newZoomedInCameraVFOVInDegrees = HandleFOVInput();

            newHipfireAndCutscenesCameraHFOVInRadians = DegToRad(newHipfireAndCutscenesCameraHFOVInDegrees);

            newHipfireAndCutscenesCameraVFOVInRadians = DegToRad(newHipfireAndCutscenesCameraVFOVInDegrees);

            newZoomedInCameraHFOVInRadians = DegToRad(newZoomedInCameraHFOVInDegrees);

            newZoomedInCameraVFOVInRadians = DegToRad(newZoomedInCameraVFOVInDegrees);

            descriptor1 = "changed";

            descriptor2 = " to:\nHipfire and Cutscenes FOV: " + to_string(newHipfireAndCutscenesCameraHFOVInDegrees) + "\u00B0 (Horizontal); " + to_string(newHipfireAndCutscenesCameraVFOVInDegrees) + "\u00B0 (Vertical)\nZoomed-in FOV: " + to_string(newZoomedInCameraHFOVInDegrees) + "\u00B0 (Horizontal); " + to_string(newZoomedInCameraVFOVInDegrees) + "\u00B0 (Vertical)";

            break;
        }

        newHipfireAndCutscenesCameraHFOVInRadiansAsFloat = static_cast<float>(newHipfireAndCutscenesCameraHFOVInRadians);

        newHipfireAndCutscenesCameraVFOVInRadiansAsFloat = static_cast<float>(newHipfireAndCutscenesCameraVFOVInRadians);

        newZoomedInCameraHFOVInRadiansAsFloat = static_cast<float>(newZoomedInCameraHFOVInRadians);

        newZoomedInCameraVFOVInRadiansAsFloat = static_cast<float>(newZoomedInCameraVFOVInRadians);

        SearchAndReplacePatterns(file);

        file.seekp(kHipfireAndCutscenesCameraHFOVOffset);
        file.write(reinterpret_cast<char *>(&newHipfireAndCutscenesCameraHFOVInRadiansAsFloat), sizeof(newHipfireAndCutscenesCameraHFOVInRadiansAsFloat));

        file.seekp(kHipfire2AndCutscenesCameraHFOVOffset);
        file.write(reinterpret_cast<char *>(&newHipfireAndCutscenesCameraHFOVInRadiansAsFloat), sizeof(newHipfireAndCutscenesCameraHFOVInRadiansAsFloat));

        file.seekp(kHipfireAndCutscenesCameraVFOVOffset);
        file.write(reinterpret_cast<char *>(&newHipfireAndCutscenesCameraVFOVInRadiansAsFloat), sizeof(newHipfireAndCutscenesCameraVFOVInRadiansAsFloat));

        file.seekp(kZoomedInCameraHFOVOffset);
        file.write(reinterpret_cast<char *>(&newZoomedInCameraHFOVInRadiansAsFloat), sizeof(newZoomedInCameraHFOVInRadiansAsFloat));

        file.seekp(kZoomedInCameraVFOVOffset);
        file.write(reinterpret_cast<char *>(&newZoomedInCameraVFOVInRadiansAsFloat), sizeof(newZoomedInCameraVFOVInRadiansAsFloat));

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