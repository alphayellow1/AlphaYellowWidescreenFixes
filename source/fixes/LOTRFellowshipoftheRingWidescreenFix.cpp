#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type
#include <cmath>
#include <limits>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>
#include <cstring>

using namespace std;

// Variables
int choice, tempChoice, simplifiedWidth, simplifiedHeight, aspectRatioGCD;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float newCameraFOVasFloat, newAspectRatioAsFloat;
double newCameraFOVasDouble, newAspectRatio;
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
            // Finds the first unknown byte
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
    return -1; // Returns -1 if the pattern is not found
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

        newAspectRatio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

        newAspectRatioAsFloat = static_cast<float>(newAspectRatio);

        newCameraFOVasFloat = static_cast<float>(64.0 / (newAspectRatio * 0.75));

        newCameraFOVasDouble = static_cast<double>(newCameraFOVasFloat);        

        streampos kAspectRatioOffset = FindAddress("\x4E\x00\x00\x20\x40\x00\x00\xC0\x3F\xDB\x0F\x49\x40\xA0", "xxxxx????xxxxx");

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));        

        streampos kCameraFOVOffset1 = FindAddress("\x3F\x00\xFE\xFF\x46\x00\x00\x80\x42\x00\x00\x00\x20\x00", "xxxxx????xxxxx");

        file.seekp(kCameraFOVOffset1);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset2 = FindAddress("\x40\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x50\x40\x70\x4F\x4A\x00\xE0", "xxxxx????????xxxxx");

        file.seekp(kCameraFOVOffset2);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasDouble), sizeof(newCameraFOVasDouble));

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