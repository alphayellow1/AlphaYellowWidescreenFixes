#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kHFOVOffset1 = 0x0001AC16;
const streampos kHFOVOffset2 = 0x0001AC8E;
const streampos kHFOVOffset3 = 0x0001AD19;
const streampos kHFOVOffset4 = 0x0001ADA4;
const streampos kHUDOffset1 = 0x0001AC60;
const streampos kHUDOffset2 = 0x0001ACDB;
const streampos kHUDOffset3 = 0x000AEA6F;
const streampos kHUDOffset4 = 0x000AE9EB;
const streampos kHUDOffset5 = 0x000AE96A;
const streampos kHUDOffset6 = 0x000AE8E6;
const streampos kHUDOffset7 = 0x000AE88B;

// Variables
int choice, fileOpened, tempChoice;
int16_t newWidth, newHeight;
fstream file;
bool fileNotFound, validKeyPressed;
float newHFOV, hudPosition1, hudPosition2;
double newCustomResolutionValue;
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
    fileOpened = 0; // Initializes fileOpened to 0

    file.open("kao.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {

        // Tries to open the file again
        file.open("kao.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open kao.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nkao.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "Kao The Kangaroo (2000) HUD & FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        cout << "\nEnter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\nEnter the desired height: ";
        newHeight = HandleResolutionInput();

        newHFOV = (static_cast<float>(newWidth) / static_cast<float>(newHeight)) / (4.0f / 3.0f);

        hudPosition1 = ((1.3333333333333f * 0.5f) / (static_cast<float>(newWidth) / static_cast<float>(newHeight))) * 1.1f;

        hudPosition2 = ((1.3333333333333f * 0.5f) / (static_cast<float>(newWidth) / static_cast<float>(newHeight))) * 1.2f;

        file.seekp(kHFOVOffset1);
        file.write(reinterpret_cast<const char *>(&newHFOV), sizeof(newHFOV));

        file.seekp(kHFOVOffset2);
        file.write(reinterpret_cast<const char *>(&newHFOV), sizeof(newHFOV));

        file.seekp(kHFOVOffset3);
        file.write(reinterpret_cast<const char *>(&newHFOV), sizeof(newHFOV));

        file.seekp(kHFOVOffset4);
        file.write(reinterpret_cast<const char *>(&newHFOV), sizeof(newHFOV));

        file.seekp(kHUDOffset1);
        file.write(reinterpret_cast<const char *>(&hudPosition1), sizeof(hudPosition1));

        file.seekp(kHUDOffset2);
        file.write(reinterpret_cast<const char *>(&hudPosition1), sizeof(hudPosition1));

        file.seekp(kHUDOffset3);
        file.write(reinterpret_cast<const char *>(&hudPosition2), sizeof(hudPosition2));

        file.seekp(kHUDOffset4);
        file.write(reinterpret_cast<const char *>(&hudPosition2), sizeof(hudPosition2));

        file.seekp(kHUDOffset5);
        file.write(reinterpret_cast<const char *>(&hudPosition2), sizeof(hudPosition2));

        file.seekp(kHUDOffset6);
        file.write(reinterpret_cast<const char *>(&hudPosition2), sizeof(hudPosition2));

        file.seekp(kHUDOffset7);
        file.write(reinterpret_cast<const char *>(&hudPosition2), sizeof(hudPosition2));

        cout << "\nSuccessfully changed the field of view." << endl;

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