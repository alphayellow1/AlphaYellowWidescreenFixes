#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kResolutionWidthOffset = 0x000ADAC1;
const streampos kResolutionHeightOffset = 0x000ADAD1;
const streampos kAspectRatioOffset = 0x00036251;
const streampos kFOVOffset = 0x000ADB10;

// Variables
int choice, tempChoice;
int16_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float desiredFOV, desiredAspectRatio;
double newCustomResolutionValue;
fstream file;
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
    
    file.open("GreatQuest.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {

        // Tries to open the file again
        file.open("GreatQuest.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open GreatQuest.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nGreatQuest.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "Frogger: The Great Quest (2002) Widescreen Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        cout << "\nEnter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\nEnter the desired height: ";
        newHeight = HandleResolutionInput();

        desiredAspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);

        if (desiredAspectRatio > 3.555f)
        {
            desiredFOV = ((desiredAspectRatio * 0.5f) / 2.37037037037f) * 0.83198f;
        }
        else
        {
            desiredFOV = 0.5f;
        }

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&desiredAspectRatio), sizeof(desiredAspectRatio));

        file.seekp(kFOVOffset);
        file.write(reinterpret_cast<const char *>(&desiredFOV), sizeof(desiredFOV));

        cout << "\nSuccessfully changed the resolution and field of view." << endl;

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
    } while (choice != 1); // Checks the flag in the loop condition
}