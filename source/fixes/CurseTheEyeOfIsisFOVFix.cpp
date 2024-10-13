#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <conio.h> // For getch() function [get character]
#include <cstdint> // For uint32_t variable type
#include <limits>
#include <string>
#include <cstring>
#include <algorithm>

using namespace std;

// Variables
uint32_t currentWidth, currentHeight, newWidth, newHeight;
string input, descriptor1, descriptor2;
fstream file;
int choice1, choice2, tempChoice;
bool fileNotFound, validKeyPressed;
float newAspectRatioAsFloat, newCameraFOVasFloat;
double newCameraFOV, newCameraFOVValue, newAspectRatio;
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
        customFOV = stod(input);

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
    newCameraFOVValue = ((static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0)) * 0.5;
    return newCameraFOVValue;
}

int main()
{
    cout << "Curse: The Eye of Isis (2003) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "Curse.exe");

        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newAspectRatio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

        cout << "\n- Do you want to set the field of view automatically based on the resolution set above (1) or set a custom field of view multiplier value (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            newCameraFOV = NewCameraFOVCalculation(newWidth, newHeight);
            descriptor1 = "fixed";
            descriptor2 = ".";
            break;

        case 2:
            cout << "\n- Enter the desired field of view multiplier (default value for the 4:3 aspect ratio is 0.5): ";
            HandleFOVInput(newCameraFOV);
            descriptor1 = "changed";
            descriptor2 = " to " + to_string(newCameraFOV) + ".";
            break;
        }

        newAspectRatioAsFloat = static_cast<float>(newAspectRatio);

        newCameraFOVasFloat = static_cast<float>(newCameraFOV);

        streampos kAspectRatioOffset = FindAddress("\x8B\x44\x24\x04\x68\xAB\xAA\xAA\x3F\x68", "xxxxx????x");

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kCameraFOVOffset = FindAddress("\x68\x00\x00\x00\x3F\x8B\x11\x50\xFF\x52\x18\x8B\xC8\xE8\x5F", "x????xxxxxxxxxx");

        file.seekp(kCameraFOVOffset);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully " << descriptor1 << " the field of view" << descriptor2 << endl;
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
            cout << "\nPress Enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n-----------------------------------------\n";
    } while (choice2 != 1); // Checks the flag in the loop condition
}