#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>
#include <windows.h>
#include <vector>
#include <cstring>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const float kPi = 3.14159265358979323846f;
const double kTolerance = 0.01;
const streampos kResolutionWidthOffset = 0x00021B81;
const streampos kResolutionHeightOffset = 0x00021B88;
const streampos kMainMenuAspectRatioOffset = 0x00004A23;
const streampos kAspectRatio2Offset = 0x00004A23;
const streampos kMainMenuFOVOffset = 0x00004A2D;
const streampos kFOV2Offset = 0x0000DA61;
const streampos kGameplayHFOVOffset = 0x0013D3E8;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight, newCustomResolutionValue;
string input;
fstream file, file2;
int choice, tempChoice;
bool fileNotFound, validKeyPressed, isGameplayHFOVKnown;
float newAspectRatio, customFOV, oldWidth = 4.0f, oldHeight = 3.0f, oldHFOV, oldAspectRatio = oldWidth / oldHeight,
                                 currentFOVInDegrees, newMainMenuFOV, newFOV2, newGameplayHFOV;
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
            cout << "\n"
                 << filename << " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

void SearchAndReplacePatterns(fstream &file)
{
    // Defines the original and new patterns with their sizes
    vector<pair<const char *, size_t>> patterns = {
        {"\x89\x10\x89\x70\x04", 5},
        {"\x51\x6B\xFF\xFF\xB8\xA8\x75\x42\x00\xE9\x85\x7F\xFE\xFF\xCC\xCC\x8D\x4D\xE0\xE9\x68\x24\xFF\xFF\xB8\x10\x76\x42\x00\xE9\x71\x7F\xFE\xFF\xCC\xCC\x8B\x4D\xF0\xE9\x29\x6B\xFF\xFF\xB8\x38\x76\x42\x00\xE9\x5D\x7F\xFE\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 100}};

    vector<pair<const char *, size_t>> replacements = {
        {"\xE9\xE7\x88\x01\x00", 5},
        {"\x51\x6B\xFF\xFF\xB8\xA8\x75\x42\x00\xE9\x85\x7F\xFE\xFF\xCC\xCC\x8D\x4D\xE0\xE9\x68\x24\xFF\xFF\xB8\x10\x76\x42\x00\xE9\x71\x7F\xFE\xFF\xCC\xCC\x8B\x4D\xF0\xE9\x29\x6B\xFF\xFF\xB8\x38\x76\x42\x00\xE9\x5D\x7F\xFE\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xBA\x20\x03\x00\x00\x89\x10\xBE\x58\x02\x00\x00\x89\x70\x04\xE9\x05\x77\xFE\xFF", 100}};

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
    SetConsoleOutputCP(CP_UTF8);

    cout << "Hugo: Bukkazoom! (2003) Widescreen Fixer v1.1 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        do
        {
            cout << "\n- Enter the desired width: ";
            newWidth = HandleResolutionInput();

            cout << "\n- Enter the desired height: ";
            newHeight = HandleResolutionInput();

            newAspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);

            if (newAspectRatio == 1.25) // 5:4
            {
                newGameplayHFOV = 0.65f;
                isGameplayHFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 1.33333333) < kTolerance) // 4:3
            {
                newGameplayHFOV = 0.75f;
                isGameplayHFOVKnown = true;
            }
            else if (newAspectRatio == 1.5625) // 25:16
            {
                newGameplayHFOV = 1.0f;
                isGameplayHFOVKnown = true;
            }
            else if (newAspectRatio == 1.6) // 16:10
            {
                newGameplayHFOV = 1.05000005245209f;
                isGameplayHFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 1.66666666) < kTolerance) // 15:9
            {
                newGameplayHFOV = 1.125f;
                isGameplayHFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 1.77777777) < kTolerance) // 16:9
            {
                newGameplayHFOV = 1.25f;
                isGameplayHFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 2.370370370370) < kTolerance) // 21:9 (2560x1080)
            {
                newGameplayHFOV = 1.91650009155273f;
                isGameplayHFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 3.555555555) < kTolerance) // 32:9
            {
                newGameplayHFOV = 3.25f;
                isGameplayHFOVKnown = true;
            }
            else if (fabs(newAspectRatio - 5.333333333) < kTolerance) // 48:9
            {
                newGameplayHFOV = 5.25f;
                isGameplayHFOVKnown = true;
            }
            else
            {
                cout << "\nThis aspect ratio isn't yet supported by the fixer, please insert a resolution with an aspect ratio that this fixer supports (5:4, 4:3, 25:16, 16:10, 15:9, 16:9, 21:9 (2560x1080), 32:9 or 48:9)." << endl;
                isGameplayHFOVKnown = false;
            }
        } while (!isGameplayHFOVKnown);

        oldHFOV = 45.0f;
        newMainMenuFOV = 2.0f * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0f))));

        oldHFOV = 75.0f;
        newFOV2 = 2.0f * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0f))));

        OpenFile(file, "Config.exe");

        SearchAndReplacePatterns(file);

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.close();

        OpenFile(file, "Runtime.exe");

        file.seekp(kMainMenuAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kAspectRatio2Offset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kMainMenuFOVOffset);
        file.write(reinterpret_cast<const char *>(&newMainMenuFOV), sizeof(newMainMenuFOV));

        file.seekp(kFOV2Offset);
        file.write(reinterpret_cast<const char *>(&newFOV2), sizeof(newFOV2));

        file.seekp(kGameplayHFOVOffset);
        file.write(reinterpret_cast<const char *>(&newGameplayHFOV), sizeof(newGameplayHFOV));

        cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << " and fixed the field of view." << endl;

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