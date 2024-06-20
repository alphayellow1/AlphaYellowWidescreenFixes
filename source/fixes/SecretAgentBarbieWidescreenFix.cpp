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
const streampos kFOVOffset = 0x001448C0;
const streampos kWidthSmallOffset = 0x001448A3;
const streampos kWidthBigOffset = 0x001448A4;
const streampos kHeightSmallOffset = 0x001448AE;
const streampos kHeightBigOffset = 0x001448AF;

// Variables
int oldWidth, oldHeight, newWidth, newHeight, choice, tempChoice, newCustomResolutionValue;
bool fileFound, validKeyPressed;
float newFOV;
uint8_t oldWidthSmall, oldWidthBig, oldHeightSmall, oldHeightBig, newWidthSmall, newWidthBig, newHeightSmall, newHeightBig;
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

int HandleResolutionInput()
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

    file.open("SecretAgent.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileFound to false
    if (!file.is_open())
    {
        fileFound = false;
    }

    // Loops until the file is found and opened
    while (!fileFound)
    {
        // Tries to open the file again
        file.open("SecretAgent.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open SecretAgent.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nSecretAgent.exe opened successfully!";
            fileFound = true; // Sets fileFound to true as the file is found and opened
        }
    }
}

int main()
{
    cout << "Secret Agent Barbie (2001) Widescreen Fixer v1.1 by AlphaYellow and Automaniak, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kWidthSmallOffset);
        file.read(reinterpret_cast<char *>(&oldWidthSmall), sizeof(oldWidthSmall));
        file.seekg(kWidthBigOffset);
        file.read(reinterpret_cast<char *>(&oldWidthBig), sizeof(oldWidthBig));
        file.seekg(kHeightSmallOffset);
        file.read(reinterpret_cast<char *>(&oldHeightSmall), sizeof(oldHeightSmall));
        file.seekg(kHeightBigOffset);
        file.read(reinterpret_cast<char *>(&oldHeightBig), sizeof(oldHeightBig));

        oldWidth = oldWidthSmall + oldWidthBig * 256;
        oldHeight = oldHeightSmall + oldHeightBig * 256;

        cout << "\nYour current resolution: " << int(oldWidth) << "x" << int(oldHeight) << "\n";

        cout << "\n- Type your new resolution width: ";
        newWidth = HandleResolutionInput();
        cout << "\n- Type your new resolution height: ";
        newHeight = HandleResolutionInput();

        newWidthSmall = newWidth % 256;
        newWidthBig = (newWidth - newWidthSmall) / 256;
        newHeightSmall = newHeight % 256;
        newHeightBig = (newHeight - newHeightSmall) / 256;

        newFOV = (static_cast<float>(newWidth) / static_cast<float>(newHeight) * 0.375f - 0.5f) / 2.0f + 0.5f;

        file.seekp(kWidthSmallOffset);
        file.write(reinterpret_cast<char *>(&newWidthSmall), sizeof(newWidthSmall));
        
        file.seekp(kWidthBigOffset);
        file.write(reinterpret_cast<char *>(&newWidthBig), sizeof(newWidthBig));
        
        file.seekp(kHeightSmallOffset);
        file.write(reinterpret_cast<char *>(&newHeightSmall), sizeof(newHeightSmall));
        
        file.seekp(kHeightBigOffset);
        file.write(reinterpret_cast<char *>(&newHeightBig), sizeof(newHeightBig));
        
        file.seekp(kFOVOffset);
        file.write(reinterpret_cast<const char *>(&newFOV), sizeof(newFOV));

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
    } while (choice == 2); // Checks the flag in the loop condition
}
