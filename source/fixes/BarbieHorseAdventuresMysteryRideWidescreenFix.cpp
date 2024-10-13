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
string input, descriptor;
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

double NewFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraFOVValue = ((static_cast<double>(newWidthValue) / static_cast<double>(newHeightValue)) / (4.0 / 3.0)) * 0.5;
    return newCameraFOVValue;
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

int main()
{
    cout << "Barbie Horse Adventures: Mystery Ride (2003) Widescreen Fixer v1.1 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "Barbie Horse.exe");

        streampos kResolutionWidthOffset = FindAddress("\xC0\x80\x02\x00\x00\x7C", "x????x");

        streampos kResolutionHeightOffset = FindAddress("\xC4\xE0\x01\x00\x00\x7C", "x????x");

        file.seekg(kResolutionWidthOffset);
        file.read(reinterpret_cast<char *>(&currentWidth), sizeof(currentWidth));

        file.seekg(kResolutionHeightOffset);
        file.read(reinterpret_cast<char *>(&currentHeight), sizeof(currentHeight));

        cout << "\nCurrent resolution is " << currentWidth << "x" << currentHeight << endl;

        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newAspectRatio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

        cout << "\n- Do you want to set the field of view automatically based on the resolution set above (1) or set a custom field of view value (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            newCameraFOV = NewFOVCalculation(newWidth, newHeight);
            descriptor = "fixed";
            break;

        case 2:
            cout << "\n- Enter the desired field of view multiplier (default value for the 4:3 aspect ratio is 0.5, beware that changing this value beyond the automatic value for the aspect ratio also increases field of view in cutscenes): ";
            HandleFOVInput(newCameraFOV);
            descriptor = "changed";
            break;
        }

        newAspectRatioAsFloat = static_cast<float>(newAspectRatio);

        newCameraFOVasFloat = static_cast<float>(newCameraFOV);

        file.seekp(kResolutionWidthOffset);
        file.write(reinterpret_cast<const char *>(&newWidth), sizeof(newWidth));

        file.seekp(kResolutionHeightOffset);
        file.write(reinterpret_cast<const char *>(&newHeight), sizeof(newHeight));

        streampos kAspectRatioOffset1 = FindAddress("\x75\x00\xA3\xF0\xA8\x76\x00\x68\xAB\xAA\xAA\x3F", "xxxxxxxx????");

        file.seekp(kAspectRatioOffset1);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset2 = FindAddress("\x8A\xF7\xFF\x83\xC4\x0C\x68\xAB\xAA\xAA\x3F", "xxxxxxx????");

        file.seekp(kAspectRatioOffset2);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));        

        streampos kAspectRatioOffset3 = FindAddress("\x26\xF7\xFF\x83\xC4\x0C\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxx????x");

        file.seekp(kAspectRatioOffset3);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset4 = FindAddress("\xA3\xC4\x31\x77\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxx????x");

        file.seekp(kAspectRatioOffset4);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset5 = FindAddress("\xD8\xE9\x98\x00\x00\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxx????x");

        file.seekp(kAspectRatioOffset5);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset6 = FindAddress("\x04\xA3\xF0\xA8\x76\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxx????x");

        file.seekp(kAspectRatioOffset6);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset7 = FindAddress("\xED\xFF\x83\xC4\x04\x89\x45\xF8\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxxxx????x");

        file.seekp(kAspectRatioOffset7);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset8 = FindAddress("\x15\xF0\xA8\x76\x00\x89\x15\xC4\x31\x77\x00\x68\xAB\xAA\xAA\x3F", "xxxxxxxxxxxx????");

        file.seekp(kAspectRatioOffset8);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset9 = FindAddress("\xEC\xFF\x83\xC4\x04\xA3\xC4\x31\x77\x00\x8B\x15\xC4\x31\x77\x00\x89\x15\xF0\xA8\x76\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxxxxxxxxxxxxxxxxxx????x");

        file.seekp(kAspectRatioOffset9);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset10 = FindAddress("\x04\x8B\x0D\xF0\xA8\x76\x00\x89\x0D\xC4\x31\x77\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxxxxxxxxx????x");

        file.seekp(kAspectRatioOffset10);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset11 = FindAddress("\xD6\xEB\xFF\x83\xC4\x04\xA3\xC4\x31\x77\x00\x8B\x15\xC4\x31\x77\x00\x89\x15\xF0\xA8\x76\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxxxxxxxxxxxxxxxxxxx????x");

        file.seekp(kAspectRatioOffset11);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset12 = FindAddress("\x00\x51\xE8\x58\xB5\xEB\xFF\x83\xC4\x04\xA3\xC4\x31\x77\x00\x8B\x15\xC4\x31\x77\x00\x89\x15\xF0\xA8\x76\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxxxxxxxxxxxxxxxxxxxxxxx????x");

        file.seekp(kAspectRatioOffset12);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset13 = FindAddress("\x0D\xF0\xA8\x76\x00\x68\xAB\xAA\xAA\x3F", "xxxxxx????");

        file.seekp(kAspectRatioOffset13);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kAspectRatioOffset14 = FindAddress("\x00\x52\xE8\x9F\x5C\xEB\xFF\x83\xC4\x04\xA3\xC4\x31\x77\x00\xA1\xC4\x31\x77\x00\xA3\xF0\xA8\x76\x00\x68\xAB\xAA\xAA\x3F\x68", "xxxxxxxxxxxxxxxxxxxxxxxxxx????x");

        file.seekp(kAspectRatioOffset14);
        file.write(reinterpret_cast<const char *>(&newAspectRatioAsFloat), sizeof(newAspectRatioAsFloat));

        streampos kCameraFOVOffset1 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x0D\x48", "x????xxx");

        file.seekp(kCameraFOVOffset1);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset2 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x15\xC8\x31\x77\x00\x52\xA1\xF0\xA8\x76\x00\x50\xE8\xB5", "x????xxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset2);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset3 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x0D\xC8\x31\x77\x00\x51\x8B\x15\xF0\xA8\x76\x00\x52\xE8\x7E", "x????xxxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset3);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset4 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x0D\xC8\x31\x77\x00\x51\x8B\x15\xF0\xA8\x76\x00\x52\xE8\xCE", "x????xxxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset4);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset5 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x45", "x????xx");

        file.seekp(kCameraFOVOffset5);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset6 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x0D\xC8\x31\x77\x00\x51\x8B\x15\xF0\xA8\x76\x00\x52\xE8\x8A", "x????xxxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset6);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset7 = FindAddress("\x68\x00\x00\x00\x3F\xA1\xC8\x31\x77\x00\x50\x8B\x4D", "x????xxxxxxxx");

        file.seekp(kCameraFOVOffset7);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset8 = FindAddress("\x68\x00\x00\x00\x3F\xA1\xC8\x31\x77\x00\x50\x8B\x0D\xC4", "x????xxxxxxxxx");

        file.seekp(kCameraFOVOffset8);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset9 = FindAddress("\x68\x00\x00\x00\x3F\xA1\xC8\x31\x77\x00\x50\x8B\x0D\xF0\xA8\x76\x00\x51\xE8\x2A", "x????xxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset9);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset10 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x15\xC8\x31\x77\x00\x52\xA1\xF0\xA8\x76\x00\x50\xE8\x97", "x????xxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset10);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset11 = FindAddress("\x68\x00\x00\x00\x3F\xA1\xC8\x31\x77\x00\x50\x8B\x0D\xF0\xA8\x76\x00\x51\xE8\x0E", "x????xxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset11);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset12 = FindAddress("\x68\x00\x00\x00\x3F\xA1\xC8\x31\x77\x00\x50\x8B\x0D\xF0\xA8\x76\x00\x51\xE8\xD2", "x????xxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset12);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset13 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x15\xC8\x31\x77\x00\x52\xA1\xF0\xA8\x76\x00\x50\xE8\x6B", "x????xxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset13);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        streampos kCameraFOVOffset14 = FindAddress("\x68\x00\x00\x00\x3F\x8B\x0D\xC8\x31\x77\x00\x51\x8B\x15\xF0\xA8\x76\x00\x52\xE8\x1A", "x????xxxxxxxxxxxxxxxx");

        file.seekp(kCameraFOVOffset14);
        file.write(reinterpret_cast<const char *>(&newCameraFOVasFloat), sizeof(newCameraFOVasFloat));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the resolution to " << newWidth << "x" << newHeight << " and " << descriptor << " the field of view." << endl;
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