#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h>
#include <cstdint>
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kResolutionWidthOffset = 0x000B77C1;
const streampos kResolutionHeightOffset = 0x000B77D1;
const streampos kHFOVOffset = 0x00086057;
const streampos kClippingFixOffset = 0x0015F6E0;

// Variables
int16_t currentWidth, currentHeight, newWidth, newHeight, newCustomResolutionValue;
fstream file;
int choice, tempChoice;
bool fileNotFound, validKeyPressed;
float newHFOV, clippingFix;
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

// Function to handle user input in resolution
int16_t HandleResolutionInput()
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
void OpenFile(fstream &file)
{
    fileNotFound = false;
    
    file.open("KaosPC.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {

        // Tries to open the file again
        file.open("KaosPC.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open KaosPC.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nKaosPC.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "State of Emergency (2003) Widescreen Fixer v1.3 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kResolutionWidthOffset);
        file.read(reinterpret_cast<char *>(&currentWidth), sizeof(currentWidth));

        file.seekg(kResolutionHeightOffset);
        file.read(reinterpret_cast<char *>(&currentHeight), sizeof(currentHeight));

        cout << "\nYour current resolution is " << currentWidth << "x" << currentHeight << "." << endl;

        cout << "\n- Enter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\n- Enter the desired height: ";
        newHeight = HandleResolutionInput();

        newHFOV = static_cast<float>(newWidth) / static_cast<float>(newHeight);

        clippingFix = (4.0f / 3.0f) / (static_cast<float>(newWidth) / static_cast<float>(newHeight));

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(kHFOVOffset);
        file.write(reinterpret_cast<const char *>(&newHFOV), sizeof(newHFOV));

        file.seekp(kClippingFixOffset);
        file.write(reinterpret_cast<const char *>(&clippingFix), sizeof(clippingFix));

        // Confirmation message
        cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << " and fixed the field of view." << endl;

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