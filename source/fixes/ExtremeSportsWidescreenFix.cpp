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
const streampos kGameplayCameraFOVOffset = 0x001D3565;
const streampos kMenuCameraFOVOffset = 0x001D3575;
const streampos kAspectRatioOffset = 0x001D45FC;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float newAspectRatioAsFloat, newGameplayCameraFOVasFloat, newMenuCameraFOVasFloat;
double newAspectRatio, newGameplayCameraFOV, newMenuCameraFOV;
string input;
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

void HandleFOVInput(double &newCustomFOV)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
        newCustomFOV = stod(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newCustomFOV <= 0)
        {
            cout << "Please enter a valid number for the FOV multiplier (greater than 0)." << endl;
        }
    } while (newCustomFOV <= 0);
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

double NewAspectRatioCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    return (static_cast<double>(newWidth) / static_cast<double>(newHeight)) * 0.75 * 4.0;
}

double NewGameplayCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    return 1.428147912 / ((4.0 / 3.0) / (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)));
}

double NewMenuCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    return 0.8000000119 / ((4.0 / 3.0) / (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)));
}

int main()
{
    cout << "Extreme Sports (2000) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newAspectRatio = NewAspectRatioCalculation(newWidth, newHeight);

        newMenuCameraFOV = NewMenuCameraFOVCalculation(newWidth, newHeight);

        cout << "\n- Do you want to set camera FOV automatically based on the resolution typed above (1) or set a custom multiplier value (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            // Calculates the new camera FOV
            newGameplayCameraFOV = NewGameplayCameraFOVCalculation(newWidth, newHeight);
            break;

        case 2:
            cout << "\n- Enter the desired camera FOV multiplier (default for 4:3 aspect ratio is 1.428147912): ";
            HandleFOVInput(newGameplayCameraFOV);
            break;
        }

        newAspectRatioAsFloat = static_cast<float>(newAspectRatio);

        newMenuCameraFOVasFloat = static_cast<float>(newMenuCameraFOV);

        newGameplayCameraFOVasFloat = static_cast<float>(newGameplayCameraFOV);

        OpenFile(file, "pc.exe");
        
        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        file.seekp(kGameplayCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newGameplayCameraFOVasFloat), sizeof(newGameplayCameraFOVasFloat));

        file.seekp(kMenuCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newMenuCameraFOVasFloat), sizeof(newMenuCameraFOVasFloat));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the aspect ratio and field of view." << endl;
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
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n-----------------------------------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}