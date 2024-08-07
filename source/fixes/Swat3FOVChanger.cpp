#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <windows.h>
#include <cmath>
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kFOVOffset = 0x0025CB90;
const float kDefaultFOV = 0.5f;

// Variables
fstream file;
string input;
int choice, tempChoice;
bool fileNotFound, validKeyPressed;
float currentFOVInMultiplier, currentFOVInDegrees, newFOVInMultiplier, newFOVInDegrees, customFOV;
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
        else if (customFOV <= 0 || customFOV >= 180)
        {
            cout << "Please enter a valid number for the FOV (greater than 0 and less than 180)." << endl;
        }
    } while (customFOV <= 0 || customFOV >= 180);

    return customFOV;
}

// Function to open the file
void OpenFile(fstream &file)
{
    fileNotFound = false;
    
    file.open("swat.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {
        // Tries to open the file again
        file.open("swat.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open swat.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nswat.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

// Function to search and replace a byte pattern
void SearchAndReplacePattern(fstream &file)
{
    // Defines the original and new patterns
    const char originalPattern[] = "\xD8\x0D\x94\xE2\x65\x00\x8B\x54\x24\x08\xD9\xF2\xDD\xD8\xD9\x5C";
    const char newPattern[] = "\xD8\x0D\x90\xCB\x65\x00\x8B\x54\x24\x08\xD9\xF2\xDD\xD8\xD9\x5C";
    const size_t patternSize = sizeof(originalPattern) - 1; // -1 to exclude the null terminator

    // Reads the entire file content into memory
    file.seekg(0, ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, ios::beg);
    char *buffer = new char[fileSize];
    file.read(buffer, fileSize);

    // Searches for the pattern
    char *patternLocation = search(buffer, buffer + fileSize, originalPattern, originalPattern + patternSize);

    // If the pattern is found, replace it
    if (patternLocation != buffer + fileSize)
    {
        memcpy(patternLocation, newPattern, patternSize);

        // Writes the modified content back to the file
        file.seekp(patternLocation - buffer);
        file.write(newPattern, patternSize);

        file.seekp(kFOVOffset);
        file.write(reinterpret_cast<const char *>(&kDefaultFOV), sizeof(kDefaultFOV));

        file.flush();
    }

    // Cleans it up
    delete[] buffer;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "SWAT 3: Close Quarters Battle (1999) FOV Changer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        // Calls the search and replace function after opening the file
        SearchAndReplacePattern(file);

        file.seekg(kFOVOffset);
        file.read(reinterpret_cast<char *>(&currentFOVInMultiplier), sizeof(currentFOVInMultiplier));

        currentFOVInDegrees = (currentFOVInMultiplier * 80.0f) * 2.0f;

        cout << "\n- The current FOV is " << currentFOVInDegrees << "\u00B0." << endl;

        cout << "\n- Enter the FOV value (default is 80\u00B0): ";
        newFOVInDegrees = HandleFOVInput();

        newFOVInMultiplier = (newFOVInDegrees * 0.5f) / 80.0f;

        file.seekp(kFOVOffset);
        file.write(reinterpret_cast<const char *>(&newFOVInMultiplier), sizeof(newFOVInMultiplier));

        // Confirmation message
        cout << "\nSuccessfully changed the field of view to " << newFOVInDegrees << "\u00B0." << endl;

        // Closes the file
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice);

        if (choice == 1)
        {
            cout << "\nPress Enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }
    } while (choice == 2); // Checks the flag in the loop condition
}
