#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <limits>
#include <conio.h>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kFOVOffset = 0x00084FC1;
const streampos kAspectRatioOffset = 0x00084FC5;

// Variables
int choice1, choice2, tempChoice;
uint32_t desiredWidth, desiredHeight, newCustomResolutionValue;
fstream file;
string input;
bool fileFound, validKeyPressed;
float customFOV, newFOV, newAspectRatio;
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
        else if (customFOV <= 0)
        {
            cout << "Please enter a valid number for the FOV multiplier (greater than 0)." << endl;
        }
    } while (customFOV <= 0);

    return customFOV;
}

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
void OpenFile(fstream &file)
{
    fileFound = true;

    file.open("IceSkating.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileFound to false
    if (!file.is_open())
    {
        fileFound = false;
    }

    // Loops until the file is found and opened
    while (!fileFound)
    {
        // Tries to open the file again
        file.open("IceSkating.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open IceSkating.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nIceSkating.exe opened successfully!";
            fileFound = true; // Sets fileFound to true as the file is found and opened
        }
    }
}

int main()
{
    cout << "Barbie Sparkling Ice Show (2002) Widescreen Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        cout << "\n- Type the desired resolution width: ";
        desiredWidth = HandleResolutionInput();

        cout << "\n- Type the desired resolution height: ";
        desiredHeight = HandleResolutionInput();

        newAspectRatio = static_cast<float>(desiredWidth) / static_cast<float>(desiredHeight);

        cout << "\n- Do you want to set FOV automatically based on the resolution typed above (1) or set a custom FOV value (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            newFOV = (static_cast<float>(desiredWidth) / static_cast<float>(desiredHeight) * 0.375f - 0.5f) / 2.0f + 0.5f;
            break;

        case 2:
            cout << "\n- Enter the desired FOV multiplier (default value for the 4:3 aspect ratio is 0.5): ";
            newFOV = HandleFOVInput();
            break;
        }

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kFOVOffset);
        file.write(reinterpret_cast<const char *>(&newFOV), sizeof(newFOV));

        cout << "\nSuccessfully fixed the aspect ratio and field of view." << endl;

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

        cout << "\n--------------------------------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}