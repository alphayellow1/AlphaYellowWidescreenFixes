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
const streampos kAspectRatioOffset = 0x0011D948;
const streampos kCameraFOVOffset1 = 0x001208CC;
const streampos kCameraFOVOffset2 = 0x00120A90;

// Variables
int choice, tempChoice, simplifiedWidth, simplifiedHeight, aspectRatioGCD;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
double newCameraFOV2;
float newCameraFOV, newAspectRatio;
fstream file;
char ch;

// Function to calculate the greatest common divisor
int gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
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

// Function to handle user input in resolution
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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\n" << filename << " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "The Lord of the Rings: The Fellowship of the Ring (2002) Widescreen Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "Fellowship.exe");

        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        // Calculates the greatest common divisor of the width and height
        aspectRatioGCD = gcd(static_cast<int>(newWidth), static_cast<int>(newHeight));

        // Simplifies the width and height by dividing by the greatest common divisor
        simplifiedWidth = static_cast<int>(newWidth) / aspectRatioGCD;
        simplifiedHeight = static_cast<int>(newHeight) / aspectRatioGCD;

        if (simplifiedWidth == 16 && simplifiedHeight == 3)
        {
            simplifiedWidth = 48;
            simplifiedHeight = 9;
        }
        else if (simplifiedWidth == 8 && simplifiedHeight == 5)
        {
            simplifiedWidth = 16;
            simplifiedHeight = 10;
        }
        else if (simplifiedWidth == 7 && simplifiedHeight == 3)
        {
            simplifiedWidth = 21;
            simplifiedHeight = 9;
        }
        else if (simplifiedWidth == 5 && simplifiedHeight == 1)
        {
            simplifiedWidth = 45;
            simplifiedHeight = 9;
        }

        newAspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);

        // Puts the FOV value in the FOV addresses (as float and double)
        newCameraFOV = 64.0f / (newAspectRatio * 0.75f);

        newCameraFOV2 = static_cast<double>(newCameraFOV);

        // Puts the aspect ratio value in the AR address
        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        file.seekp(kCameraFOVOffset1);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        file.seekp(kCameraFOVOffset2);
        file.write(reinterpret_cast<const char *>(&newCameraFOV2), sizeof(newCameraFOV2));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the aspect ratio to " << simplifiedWidth << ":" << simplifiedHeight << " and fixed the field of view. Now all " << simplifiedWidth << ":" << simplifiedHeight << " resolutions supported by the graphics card's driver will appear in the game's video settings." << endl;
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
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n-----------------------------------------\n";
    } while (choice == 2); // Checks the flag in the loop condition
}