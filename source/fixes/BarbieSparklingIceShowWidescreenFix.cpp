#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type 
#include <cmath>
#include <limits>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kCameraFOVOffset = 0x00084FC1;
const streampos kAspectRatioOffset = 0x00084FC5;

// Variables
int choice, tempChoice;
uint32_t newWidth, newHeight;
fstream file;
string input;
bool fileFound, validKeyPressed, isAspectRatioKnown;
double newCameraFOV, newAspectRatio;
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

void HandleFOVInput(double &customFOV)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
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
    fileFound = true;

    file.open(filename, ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileFound to false
    if (!file.is_open())
    {
        fileFound = false;
    }

    // Loops until the file is found and opened
    while (!fileFound)
    {
        // Tries to open the file again
        file.open(filename, ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\n" << filename << " opened successfully!";
            fileFound = true; // Sets fileFound to true as the file is found and opened
        }
    }
}

int main()
{
    cout << "Barbie Sparkling Ice Show (2002) Widescreen Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "IceSkating.exe");

        do
        {
            cout << "\n- Type the desired resolution width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Type the desired resolution height: ";
            HandleResolutionInput(newHeight);

            newAspectRatio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

            if (newAspectRatio == 4.0 / 3.0)
            {
                newCameraFOV = 0.5;
                isAspectRatioKnown = true;
            }
            else if (newAspectRatio == 16.0 / 10.0)
            {
                newCameraFOV = 0.5875;
                isAspectRatioKnown = true;
            }
            else if (newAspectRatio == 16.0 / 9.0)
            {
                newCameraFOV = 0.6427778006;
                isAspectRatioKnown = true;
            }
            else
            {
                cout << "\nThis aspect ratio is not yet supported by the fixer, try a less wide one." << endl;
                isAspectRatioKnown = false;
            }
        } while (!isAspectRatioKnown);

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully fixed the aspect ratio and field of view." << endl;
        }
        else
        {
            cout << "\nError(s) occurred during the file operations." << endl;
        }

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

        cout << "\n--------------------------------------\n";
    } while (choice == 2); // Checks the flag in the loop condition
}