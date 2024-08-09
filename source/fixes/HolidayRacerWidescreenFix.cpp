#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <limits>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kResolutionWidth1Offset = 0x0010A9E0;
const streampos kResolutionHeight1Offset = 0x0010A9E4;
const streampos kResolutionWidth2Offset = 0x0010AA58;
const streampos kResolutionHeight2Offset = 0x0010AA5C;
const streampos kResolutionWidth3Offset = 0x0010AAF8;
const streampos kResolutionHeight3Offset = 0x0010AAFC;

// Variables
int choice, tempChoice;
int32_t currentWidth, currentHeight, newWidth, newHeight, newCustomResolutionValue;
bool fileNotFound, validKeyPressed;
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
    
    file.open("spel.dat", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {
        // Tries to open the file again
        file.open("spel.dat", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open spel.dat, check if the DAT file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the file is currently being used. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nspel.dat opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "Holiday Racer (2001) Widescreen Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kResolutionWidth1Offset);
        file.read(reinterpret_cast<char *>(&currentWidth), sizeof(currentWidth));

        file.seekg(kResolutionHeight1Offset);
        file.read(reinterpret_cast<char *>(&currentHeight), sizeof(currentHeight));

        cout << "\nYour current resolution is " << currentWidth << "x" << currentHeight << "." << endl;

        cout << "\n- Enter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\n- Enter the desired height: ";
        newHeight = HandleResolutionInput();

        file.seekp(kResolutionWidth1Offset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeight1Offset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(kResolutionWidth2Offset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeight2Offset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        file.seekp(kResolutionWidth3Offset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeight3Offset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << "." << endl;

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