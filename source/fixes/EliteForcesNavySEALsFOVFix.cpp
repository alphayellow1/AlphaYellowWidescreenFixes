#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type 
#include <windows.h>
#include <cmath>
#include <limits>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>
#include <vector>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const double kDefaultCameraVerticalFOVInRadians = 1.1780972480773926;
const streampos kCameraHorizontalFOVOffset = 0x000BAA31;
const streampos kCameraVerticalFOVOffset = 0x000BAA41;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float currentCameraHorizontalFOVInRadians, currentCameraVerticalFOVInRadians;
double newCameraHorizontalFOVInRadians, newCameraVerticalFOVInRadians, currentCameraHorizontalFOVInDegrees, currentCameraVerticalFOVInDegrees, newCameraHorizontalFOVInDegrees, newCameraVerticalFOVInDegrees, oldWidth = 4.0, oldHeight = 3.0, oldCameraHorizontalFOV = 90.0, oldAspectRatio = oldWidth / oldHeight;
string descriptor, input;
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

void HandleFOVInput(double &newCustomFOVInDegrees)
{
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
}

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
        {"\x89\x81\x98\x01\x00\x00\x8B\x45\x10\x89\x81\x9C\x01\x00\x00", 15},
        // DISASSEMBLED CODE - PATTERN 1 (UNMODIFIED)
        // 00408732 | 89 81 98 01 00 00 | mov [ecx+00000198],eax
        // 00408738 | 8B 45 10          | mov eax,[ebp+10]
        // 0040873B | 89 81 9C 01 00 00 | mov [ecx+0000019C],eax

        {"\xE9\x40\x73\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 37}
    };

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\xF9\x22\x0B\x00\x90\x8B\x45\x10\xE9\x00\x23\x0B\x00\x90", 15},
        // DISASSEMBLED CODE - PATTERN 1 (MODIFIED)
        // 00408732 | E9 F9 22 0B 00 | jmp 004BAA30
        // 00408737 | 90             | nop
        // 00408738 | 8B 45 10       | mov eax,[ebp+10]
        // 0040873B | E9 00 23 0B 00 | jmp 004BAA40
        // 00408740 | 90             | nop

        {"\xE9\x40\x73\xFF\xFF\xB8\xDB\x0F\xC9\x3F\x89\x81\x98\x01\x00\x00\xE9\xF8\xDC\xF4\xFF\xB8\xE4\xCB\x96\x3F\x89\x81\x9C\x01\x00\x00\xE9\xF1\xDC\xF4\xFF", 37}
        // DISASSEMBLED CODE - PART OF PATTERN 2 (MODIFIED)
        // CODECAVE ENTRYPOINT AT 004BAA30 (x32dbg)
        // 004BAA30 | B8 DB 0F C9 3F    | mov eax,3FC90FDB
        // 004BAA35 | 89 81 98 01 00 00 | mov [ecx+00000198],eax
        // 004BAA3B | E9 F8 DC F4 FF    | jmp 00408738
        // 004BAA40 | B8 E4 CB 96 3F    | mov eax,3F96CBE4
        // 004BAA45 | 89 81 9C 01 00 00 | mov [ecx+0000019C],eax
        // 004BAA4B | E9 F1 DC F4 FF    | jmp 00408741
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

double NewHorizontalCameraFOVInDegreesCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    return 2.0 * RadToDeg(atan((static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / oldAspectRatio) * tan(DegToRad(oldCameraHorizontalFOV / 2.0)));
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Elite Forces: Navy SEALs (2002) FOV Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "lithtech.exe");

        file.seekg(kCameraHorizontalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraHorizontalFOVInRadians), sizeof(currentCameraHorizontalFOVInRadians));

        file.seekg(kCameraVerticalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentCameraVerticalFOVInRadians), sizeof(currentCameraVerticalFOVInRadians));

        file.close();

        // Converts the FOV values from radians to degrees
        currentCameraHorizontalFOVInDegrees = RadToDeg(static_cast<double>(currentCameraHorizontalFOVInRadians));
        currentCameraVerticalFOVInDegrees = RadToDeg(static_cast<double>(currentCameraVerticalFOVInRadians));

        cout << "\nCurrent FOV: " << currentCameraHorizontalFOVInDegrees << "\u00B0 (Horizontal); " << currentCameraVerticalFOVInDegrees << "\u00B0 (Vertical) " << "\n";

        cout << "\n- Do you want to set FOV automatically based on the desired resolution (1) or set custom horizontal and vertical FOV values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            // Calculates the new horizontal FOV
            newCameraHorizontalFOVInDegrees = NewHorizontalCameraFOVInDegreesCalculation(newWidth, newHeight);

            newCameraHorizontalFOVInRadians = DegToRad(newCameraHorizontalFOVInDegrees); // Converts degrees to radians

            newCameraVerticalFOVInRadians = kDefaultCameraVerticalFOVInRadians;

            newCameraVerticalFOVInDegrees = RadToDeg(newCameraVerticalFOVInRadians);

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired horizontal FOV (in degrees, default for 4:3 is 90\u00B0): ";
            HandleFOVInput(newCameraHorizontalFOVInDegrees);

            cout << "\n- Enter the desired vertical FOV (in degrees, default for 4:3 is 67.5\u00B0): ";
            HandleFOVInput(newCameraVerticalFOVInDegrees);

            newCameraHorizontalFOVInRadians = DegToRad(newCameraHorizontalFOVInDegrees); // Converts degrees to radians

            newCameraVerticalFOVInRadians = DegToRad(newCameraVerticalFOVInDegrees); // Converts degrees to radians

            descriptor = "manually";

            break;
        }

        float newCameraHorizontalFOVInRadiansAsFloat = static_cast<float>(newCameraHorizontalFOVInRadians);

        float newCameraVerticalFOVInRadiansAsFloat = static_cast<float>(newCameraVerticalFOVInRadians);

        OpenFile(file, "lithtech.exe");

        SearchAndReplacePatterns(file);

        file.seekp(kCameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOVInRadiansAsFloat), sizeof(newCameraHorizontalFOVInRadiansAsFloat));

        file.seekp(kCameraVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraVerticalFOVInRadiansAsFloat), sizeof(newCameraVerticalFOVInRadiansAsFloat));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed " << descriptor << " the horizontal FOV to " << newCameraHorizontalFOVInDegrees << "\u00B0 and vertical FOV to " << newCameraVerticalFOVInDegrees << "\u00B0."
                 << endl;
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