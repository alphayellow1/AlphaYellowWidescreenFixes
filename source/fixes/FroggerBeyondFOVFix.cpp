#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch() function [get character]
#include <cstdint> // For uint32_t variable type 
#include <cmath>
#include <limits>
#include <string>
#include <cstring>
#include <algorithm>

using namespace std;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float newCameraHorizontalFOVasFloat, newCameraVerticalFOVasFloat;
double newCameraHorizontalFOV, newCameraVerticalFOV, newCameraFOVValue;
fstream file;
string input, descriptor;
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

void HandleFOVInput(double &customFOVMultiplier)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a double
        customFOVMultiplier = stod(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (customFOVMultiplier <= 0)
        {
            cout << "Please enter a number greater than 0 for the FOV." << endl;
        }
    } while (customFOVMultiplier <= 0);
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

streampos FindAddress(const char *pattern, const char *mask)
{
    file.seekg(0, ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, ios::beg);
    char *buffer = new char[fileSize];
    file.read(buffer, fileSize);

    size_t patternSize = strlen(mask);

    for (size_t j = 0; j < fileSize - patternSize; ++j)
    {
        bool match = true;
        for (size_t k = 0; k < patternSize; ++k)
        {
            if (mask[k] == 'x' && buffer[j + k] != pattern[k])
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            // Find the first unknown byte
            for (size_t k = 0; k < patternSize; ++k)
            {
                if (mask[k] == '?')
                {
                    streampos fileOffset = j + k;
                    delete[] buffer;
                    return fileOffset;
                }
            }
        }
    }

    delete[] buffer;
    return -1; // Return -1 if pattern not found
}

double NewCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraFOVValue = 0.6999999881f / ((4.0 / 3.0) / (static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)));
    return newCameraFOVValue;
}

int main()
{
    cout << "Frogger Beyond (2003) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        cout << "\n- Do you want to fix the FOV automatically based on a desired resolution (1) or set custom multiplier values for horizontal and vertical FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            newCameraHorizontalFOV = NewCameraFOVCalculation(newWidth, newHeight);

            newCameraVerticalFOV = NewCameraFOVCalculation(newWidth, newHeight);

            descriptor = "fixed";

            break;

        case 2:
            cout << "\n- Enter a custom horizontal FOV multiplier value (default for 4:3 aspect ratio is 0.7): ";
            HandleFOVInput(newCameraHorizontalFOV);

            cout << "\n- Enter a custom vertical FOV multiplier value (default for 4:3 aspect ratio is 0.7): ";
            HandleFOVInput(newCameraVerticalFOV);

            descriptor = "changed";

            break;
        }

        newCameraHorizontalFOVasFloat = static_cast<float>(newCameraHorizontalFOV);

        newCameraVerticalFOVasFloat = static_cast<float>(newCameraVerticalFOV);

        OpenFile(file, "Frogger Beyond.exe");

        streampos kCameraHorizontalFOVOffset = FindAddress("\xC7\x45\xDC\x33\x33\x33\x3F\x83\x7D\x08", "xxx????xxx");

        file.seekp(kCameraHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraHorizontalFOVasFloat), sizeof(newCameraHorizontalFOVasFloat));

        streampos kCameraVerticalFOVOffset = FindAddress("\xF8\x3A\x40\x00\x00\x00\x00\x00\x33\x33\x33\x3F\x00\x00\x87\x43\x00\x00\x00\x00", "xxxxxxxx????xxxxxxxx");

        file.seekp(kCameraVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraVerticalFOVasFloat), sizeof(newCameraVerticalFOVasFloat));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully " << descriptor << " the field of view." << endl;
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
    } while (choice2 != 1); // Checks the flag in the loop condition
}