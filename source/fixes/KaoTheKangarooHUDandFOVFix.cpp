#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch() function [get character]
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kGameplayHorizontalFOVOffset = 0x0002D121;
const streampos kMenuHorizontalFOVOffset = 0x000CEE64;
const streampos kHUDOffset1 = 0x0001AC16;
const streampos kHUDOffset2 = 0x0001AC8E;
const streampos kHUDOffset3 = 0x0001AD19;
const streampos kHUDOffset4 = 0x0001ADA4;
const streampos kHUDOffset5 = 0x0001AC60;
const streampos kHUDOffset6 = 0x0001ACDB;
const streampos kHUDOffset7 = 0x000AEA6;
const streampos kHUDOffset8 = 0x000AE9EB;
const streampos kHUDOffset9 = 0x000AE96A;
const streampos kHUDOffset10 = 0x000AE8E6;
const streampos kHUDOffset11 = 0x000AE88B;

// Variables
int choice, tempChoice;
uint32_t newWidth, newHeight;
fstream file;
bool fileNotFound, validKeyPressed;
double newMenuHorizontalFOV, newGameplayHorizontalFOV, hudPosition1, hudPosition2, hudPosition3, newMenuHorizontalFOVValue, newGameplayHorizontalFOVValue, newHudPosition1Value, newHudPosition2Value, newHudPosition3Value;
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

double NewMenuHorizontalFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newMenuHorizontalFOVValue = (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0);
    return newMenuHorizontalFOVValue;
}

double NewGameplayHorizontalFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newGameplayHorizontalFOVValue = (4.0 / 3.0) / (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue));
    return newGameplayHorizontalFOVValue;
}

double HudPosition1Calculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newHudPosition1Value = (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0);
    return newHudPosition1Value;
}

double HudPosition2Calculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newHudPosition2Value = ((1.3333333333333f * 0.5f) / (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue))) * 1.1;
    return newHudPosition2Value;
}

double HudPosition3Calculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newHudPosition3Value = ((1.3333333333333f * 0.5f) / (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue))) * 1.2;
    return newHudPosition3Value;
}

int main()
{
    cout << "Kao The Kangaroo (2000) HUD & FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "kao.exe");

        cout << "\nEnter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\nEnter the desired height: ";
        HandleResolutionInput(newHeight);

        newMenuHorizontalFOV = NewMenuHorizontalFOVCalculation(newWidth, newHeight);

        newGameplayHorizontalFOV = NewGameplayHorizontalFOVCalculation(newWidth, newHeight);

        hudPosition1 = HudPosition1Calculation(newWidth, newHeight);

        hudPosition2 = HudPosition2Calculation(newWidth, newHeight);

        hudPosition3 = HudPosition3Calculation(newWidth, newHeight);

        file.seekp(kMenuHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newMenuHorizontalFOV), sizeof(newMenuHorizontalFOV));

        file.seekp(kGameplayHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newGameplayHorizontalFOV), sizeof(newGameplayHorizontalFOV));

        file.seekp(kHUDOffset1);
        file.write(reinterpret_cast<const char *>(&hudPosition1), sizeof(hudPosition1));

        file.seekp(kHUDOffset2);
        file.write(reinterpret_cast<const char *>(&hudPosition1), sizeof(hudPosition1));

        file.seekp(kHUDOffset3);
        file.write(reinterpret_cast<const char *>(&hudPosition1), sizeof(hudPosition1));

        file.seekp(kHUDOffset4);
        file.write(reinterpret_cast<const char *>(&hudPosition1), sizeof(hudPosition1));

        file.seekp(kHUDOffset5);
        file.write(reinterpret_cast<const char *>(&hudPosition2), sizeof(hudPosition2));

        file.seekp(kHUDOffset6);
        file.write(reinterpret_cast<const char *>(&hudPosition2), sizeof(hudPosition2));

        file.seekp(kHUDOffset7);
        file.write(reinterpret_cast<const char *>(&hudPosition3), sizeof(hudPosition3));

        file.seekp(kHUDOffset8);
        file.write(reinterpret_cast<const char *>(&hudPosition3), sizeof(hudPosition3));

        file.seekp(kHUDOffset9);
        file.write(reinterpret_cast<const char *>(&hudPosition3), sizeof(hudPosition3));

        file.seekp(kHUDOffset10);
        file.write(reinterpret_cast<const char *>(&hudPosition3), sizeof(hudPosition3));

        file.seekp(kHUDOffset11);
        file.write(reinterpret_cast<const char *>(&hudPosition3), sizeof(hudPosition3));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully fixed the field of view and HUD." << endl;
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

        cout << "\n---------------------------\n";
    } while (choice != 1); // Checks the flag in the loop condition
}