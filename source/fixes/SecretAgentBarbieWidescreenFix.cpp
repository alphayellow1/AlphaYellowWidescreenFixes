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
const streampos kCameraFOVOffset = 0x001448C0;
const streampos kWidthSmallOffset = 0x001448A3;
const streampos kWidthBigOffset = 0x001448A4;
const streampos kHeightSmallOffset = 0x001448AE;
const streampos kHeightBigOffset = 0x001448AF;

// Variables
int choice, tempChoice;
uint8_t currentWidthSmall, currentWidthBig, currentHeightSmall, currentHeightBig, newWidthSmall, newWidthBig, newHeightSmall, newHeightBig;
uint32_t currentWidth, currentHeight, newWidth, newHeight;
fstream file;
bool fileFound, validKeyPressed;
float newCameraFOV, newCameraFOVValue;
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

float NewCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraFOVValue = (static_cast<float>(newWidthValue) / static_cast<float>(newHeightValue) * 0.375f - 0.5f) / 2.0f + 0.5f;
    return newCameraFOVValue;
}

int main()
{
    cout << "Secret Agent Barbie (2001) Widescreen Fixer v1.1 by AlphaYellow and Automaniak, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "SecretAgent.exe");

        file.seekg(kWidthSmallOffset);
        file.read(reinterpret_cast<char *>(&currentWidthSmall), sizeof(currentWidthSmall));

        file.seekg(kWidthBigOffset);
        file.read(reinterpret_cast<char *>(&currentWidthBig), sizeof(currentWidthBig));

        file.seekg(kHeightSmallOffset);
        file.read(reinterpret_cast<char *>(&currentHeightSmall), sizeof(currentHeightSmall));

        file.seekg(kHeightBigOffset);
        file.read(reinterpret_cast<char *>(&currentHeightBig), sizeof(currentHeightBig));

        currentWidth = currentWidthSmall + currentWidthBig * 256;

        currentHeight = currentHeightSmall + currentHeightBig * 256;

        cout << "\nCurrent resolution: " << currentWidth << "x" << currentHeight << "\n";

        cout << "\n- Type your new resolution width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Type your new resolution height: ";
        HandleResolutionInput(newHeight);

        newWidthSmall = newWidth % 256;

        newWidthBig = (newWidth - newWidthSmall) / 256;

        newHeightSmall = newHeight % 256;

        newHeightBig = (newHeight - newHeightSmall) / 256;

        newCameraFOV = NewCameraFOVCalculation(newWidth, newHeight);

        file.seekp(kWidthSmallOffset);
        file.write(reinterpret_cast<char *>(&newWidthSmall), sizeof(newWidthSmall));
        
        file.seekp(kWidthBigOffset);
        file.write(reinterpret_cast<char *>(&newWidthBig), sizeof(newWidthBig));
        
        file.seekp(kHeightSmallOffset);
        file.write(reinterpret_cast<char *>(&newHeightSmall), sizeof(newHeightSmall));
        
        file.seekp(kHeightBigOffset);
        file.write(reinterpret_cast<char *>(&newHeightBig), sizeof(newHeightBig));
        
        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the resolution and fixed the field of view." << endl;
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