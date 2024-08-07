#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kHFOVOffset = 0x001EFAE1;
const streampos kVFOVOffset = 0x001EFAF1;
const float kDefaultVFOVMultiplier = 0.5249999762;

// Variables
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
float width, height, desiredHFOV, desiredVFOV, customFOVMultiplier;
double newCustomResolutionValue, newWidth, newHeight;
fstream file;
string input;
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
        customFOVMultiplier = stof(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (customFOVMultiplier <= 0)
        {
            cout << "Please enter a number greater than 0 for the FOV." << endl;
        }
    } while (customFOVMultiplier <= 0);

    return customFOVMultiplier;
}

// Function to handle user input in resolution
double HandleResolutionInput()
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

    file.open("Frogger Beyond.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {
        // Tries to open the file again
        file.open("Frogger Beyond.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open Frogger Beyond.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nFrogger Beyond.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "Frogger Beyond (2003) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Do you want to fix the FOV automatically based on a desired resolution (1) or set custom multiplier values for horizontal and vertical FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\nEnter the desired width: ";
            newWidth = HandleResolutionInput();

            cout << "\nEnter the desired height: ";
            newHeight = HandleResolutionInput();

            desiredHFOV = 0.6999999881f / ((4.0f / 3.0f) / (static_cast<float>(newWidth) / static_cast<float>(newHeight)));

            desiredVFOV = kDefaultVFOVMultiplier;

            break;

        case 2:
            cout << "\n- Type a custom horizontal FOV multiplier value (default for 4:3 aspect ratio is 0.7): ";
            desiredHFOV = HandleFOVInput();

            cout << "\n- Type a custom vertical FOV multiplier value (default for 4:3 aspect ratio is 0.525): ";
            desiredVFOV = HandleFOVInput();
            break;
        }

        OpenFile(file);

        file.seekp(kHFOVOffset);
        file.write(reinterpret_cast<const char *>(&desiredHFOV), sizeof(desiredHFOV));

        file.seekp(kVFOVOffset);
        file.write(reinterpret_cast<const char *>(&desiredVFOV), sizeof(desiredVFOV));

        // Confirmation message
        cout << "\nSuccessfully changed the field of view." << endl;

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

        cout << "\n-----------------------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}