#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type
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
const double kDefaultCutscenesVFOVInRadians = 0.8490677476; // 48.647998 degrees
const double kDefaultCameraVFOVInRadians = 1.1780972480773926; // 67.5 degrees
const streampos kGameplayAndCutscenesCameraHFOVOffset = 0x000C42FD;
const streampos kGameplayCameraVFOVOffset = 0x000C4309;
const streampos kCutscenesCameraVFOVOffset = 0x000C4315;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float currentCameraHFOVInRadians, currentCameraVFOVInRadians, newCameraHFOVInRadians, newCameraVFOVInRadians, DefaultCutscenesVFOVInRadiansAsFloat;
double newAspectRatio, oldWidth = 4.0, oldHeight = 3.0, oldAspectRatio = oldWidth / oldHeight, currentCameraHFOVInDegrees, currentCameraVFOVInDegrees, newCameraHFOVInDegrees, newCameraVFOVInDegrees;
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
            cout << "Please enter a valid number for the field of view (greater than 0 and less than 180)." << endl;
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
            cout << "\nFailed to open " << filename << ", check if the file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently being used. Press Enter when all the mentioned problems are solved." << endl;
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
        {"\x89\x8C\x24\x00\x01\x00\x00\x8B\x88\x98\x01\x00\x00\x89\x94\x24\xFC\x00\x00\x00", 20},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 00412D85 | 89 8C 24 00 01 00 00 | mov dword ptr ss:[esp+100],ecx
        // 00412D8C | 8B 88 98 01 00 00    | mov ecx,dword ptr ds:[eax+198]
        // 00412D92 | 89 94 24 FC 00 00 00 | mov dword ptr ss:[esp+FC],edx

        {"\x89\x94\x24\x04\x01\x00\x00\x8B\x90\x9C\x01\x00\x00\x89\x94\x24\xE4\x00\x00\x00", 20},
        // DISASSEMBLED CODE - PATTERN 2 (UNMODIFIED)
        // 00412DAC | 89 94 24 04 01 00 00 | mov dword ptr ss:[esp+104],edx
        // 00412DB3 | 8B 90 9C 01 00 00    | mov edx,dword ptr ds:[eax+19C]
        // 00412DB9 | 89 94 24 E4 00 00 00 | mov dword ptr ss:[esp+E4],edx

        {"\xE9\x9A\xE4\xFE\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 107}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\x89\x8C\x24\x00\x01\x00\x00\xE9\x28\x15\x0B\x00\x90\x89\x94\x24\xFC\x00\x00\x00", 20},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 00412D85 | 89 8C 24 00 01 00 00 | mov dword ptr ss:[esp+100],ecx
        // 00412D8C | E9 28 15 0B 00       | jmp 004C42B9
        // 00412D91 | 90                   | nop
        // 00412D92 | 89 94 24 FC 00 00 00 | mov dword ptr ss:[esp+FC],edx

        {"\x89\x94\x24\x04\x01\x00\x00\xE9\x18\x15\x0B\x00\x90\x89\x94\x24\xE4\x00\x00\x00", 20},
        // DISASSEMBLED CODE - PATTERN 2 (MODIFIED)
        // 00412DAC | 89 94 24 04 01 00 00 | mov dword ptr ss:[esp+104],edx
        // 00412DB3 | E9 18 15 0B 00       | jmp 004C42D0
        // 00412DB8 | 90                   | nop
        // 00412DB9 | 89 94 24 E4 00 00 00 | mov dword ptr ss:[esp+E4],edx

        {"\xE9\x9A\xE4\xFE\xFF\x00\x00\x00\x00\x81\xB8\x98\x01\x00\x00\xDB\x0F\xC9\x3F\x74\x32\x8B\x88\x98\x01\x00\x00\xE9\xC2\xEA\xF4\xFF\x81\xB8\x9C\x01\x00\x00\xE4\xCB\x96\x3F\x74\x27\x81\xB8\x9C\x01\x00\x00\x81\x5C\x59\x3F\x74\x27\x8B\x90\x9C\x01\x00\x00\xE9\xC6\xEA\xF4\xFF\x00\x00\x00\x00\xC7\x80\x98\x01\x00\x00\x38\x63\xED\x3F\xEB\xC2\xC7\x80\x9C\x01\x00\x00\xE4\xCB\x96\x3F\xEB\xCD\xC7\x80\x9C\x01\x00\x00\x81\x5C\x59\x3F\xEB\xCD", 107}
        // DISASSEMBLED CODE - PART OF PATTERN 3 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 004C42B9 (x32dbg)
        // 004C42B9 | 81 B8 98 01 00 00 DB 0F C9 3F | cmp dword ptr ds:[eax+198],3FC90FDB
        // 004C42C3 | 74 32                         | je 004C42F7
        // 004C42C5 | 8B 88 98 01 00 00             | mov ecx,dword ptr ds:[eax+198]
        // 004C42CB | E9 C2 EA F4 FF                | jmp 00412D92
        // 004C42D0 | 81 B8 9C 01 00 00 E4 CB 96 3F | cmp dword ptr ds:[eax+19C],3F96CBE4
        // 004C42DA | 74 27                         | je 004C4303
        // 004C42DC | 81 B8 9C 01 00 00 81 5C 59 3F | cmp dword ptr ds:[eax+19C],3F595C81
        // 004C42E6 | 74 27                         | je 004C430F
        // 004C42E8 | 8B 90 9C 01 00 00             | mov edx,dword ptr ds:[eax+19C]
        // 004C42EE | E9 C6 EA F4 FF                | jmp 00412DB9
        // 004C42F3 | 00 00                         | add byte ptr ds:[eax],al
        // 004C42F5 | 00 00                         | add byte ptr ds:[eax],al
        // 004C42F7 | C7 80 98 01 00 00 38 63 ED 3F | mov dword ptr ds:[eax+198],3FED6338
        // 004C4301 | EB C2                         | jmp 004C42C5
        // 004C4303 | C7 80 9C 01 00 00 E4 CB 96 3F | mov dword ptr ds:[eax+19C],3F96CBE4
        // 004C430D | EB CD                         | jmp 004C42DC
        // 004C430F | C7 80 9C 01 00 00 81 5C 59 3F | mov dword ptr ds:[eax+19C],3F595C81
        // 004C4319 | EB CD                         | jmp 004C42E8
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

double HorizontalFOVInDegreesCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue, double oldHorizontalFOV)
{
    return 2.0 * RadToDeg(atan(tan(DegToRad(oldHorizontalFOV / 2.0)) * (static_cast<double>(newWidthValue) / newHeightValue) / oldAspectRatio));
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Alcatraz: Prison Escape (2001) FOV Fixer v1.4 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "lithtech.exe");

        file.seekg(kGameplayAndCutscenesCameraHFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraHFOVInRadians), sizeof(currentCameraHFOVInRadians));

        file.seekg(kGameplayCameraVFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraVFOVInRadians), sizeof(currentCameraHFOVInRadians));

        // Converts the field of view values from radians to degrees
        currentCameraHFOVInDegrees = RadToDeg(currentCameraHFOVInRadians);
        currentCameraVFOVInDegrees = RadToDeg(currentCameraVFOVInRadians);

        cout << "\nCurrent field of view: " << currentCameraHFOVInDegrees << "\u00B0 (Horizontal); " << currentCameraVFOVInDegrees << "\u00B0 (Vertical)\n";

        cout << "\n- Do you want to set field of view automatically based on the desired resolution (1) or set custom horizontal and vertical field of view values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            newWidth = HandleResolutionInput();

            cout << "\n- Enter the desired height: ";
            newHeight = HandleResolutionInput();

            // Calculates the new horizontal field of view
            newCameraHFOVInDegrees = HorizontalFOVInDegreesCalculation(newWidth, newHeight, 90.0);

            newCameraHFOVInRadians = static_cast<float>(DegToRad(newCameraHFOVInDegrees)); // Converts degrees to radians

            newCameraVFOVInRadians = static_cast<float>(kDefaultCameraVFOVInRadians);

            newCameraVFOVInDegrees = RadToDeg((double)newCameraVFOVInRadians);

            descriptor1 = "fixed";

            descriptor2 = ".";

            break;

        case 2:
            cout << "\n- Enter the desired horizontal field of view (in degrees, default for 4:3 aspect ratio is 90\u00B0): ";
            newCameraHFOVInDegrees = HandleFOVInput();

            cout << "\n- Enter the desired vertical field of view (in degrees, default for 4:3 aspect ratio is 67.5\u00B0): ";
            newCameraVFOVInDegrees = HandleFOVInput();

            newCameraHFOVInRadians = static_cast<float>(DegToRad(newCameraHFOVInDegrees)); // Converts degrees to radians

            newCameraVFOVInRadians = static_cast<float>(DegToRad(newCameraVFOVInDegrees)); // Converts degrees to radians

            descriptor1 = "changed";

            descriptor2 = " to " + to_string(newCameraHFOVInDegrees) + "\u00B0 (Horizontal) and " + to_string(newCameraVFOVInDegrees) + "\u00B0 (Vertical).";

            break;
        }

        DefaultCutscenesVFOVInRadiansAsFloat = static_cast<float>(kDefaultCutscenesVFOVInRadians);

        SearchAndReplacePatterns(file);

        file.seekp(kGameplayAndCutscenesCameraHFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHFOVInRadians), sizeof(newCameraHFOVInRadians));

        file.seekp(kGameplayCameraVFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraVFOVInRadians), sizeof(newCameraHFOVInRadians));

        file.seekp(kCutscenesCameraVFOVOffset);
        file.write(reinterpret_cast<const char *>(&DefaultCutscenesVFOVInRadiansAsFloat), sizeof(DefaultCutscenesVFOVInRadiansAsFloat));

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
            cout << "\nPress Enter to exit the program...";
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}