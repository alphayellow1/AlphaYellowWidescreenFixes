#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <limits>
#include <conio.h>
#include <string>
#include <algorithm>
#include <windows.h>

using namespace std;

// Constants
const float kPi = 3.14159265358979323846f;
const float kTolerance = 0.01f;
const float kDefaultVerticalFOVInRadians = 1.1780972480773926f; // 67.5 degrees
const streampos kHorizontalFOVOffset = 0x000C42D1;
const streampos kVerticalFOVOffset = 0x000C42E6;

// Variables
int choice1, choice2, tempChoice;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float newAspectRatio, currentHorizontalFOVInRadians, currentVerticalFOVInRadians, newHorizontalFOVInRadians, newVerticalFOVInRadians, oldWidth = 4.0f, oldHeight = 3.0f, oldHorizontalFOV = 90.0f, oldAspectRatio = oldWidth / oldHeight, currentHorizontalFOVInDegrees, currentVerticalFOVInDegrees, newHorizontalFOVInDegrees, newVerticalFOVInDegrees;
string descriptor, input;
fstream file;
char ch;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
    return degrees * (kPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
    return radians * (180.0f / kPi);
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

void HandleFOVInput(float &newCustomFOVInDegrees)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a float
        newCustomFOVInDegrees = stof(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newCustomFOVInDegrees <= 0 || newCustomFOVInDegrees >= 180)
        {
            cout << "Please enter a valid number for the field of view (greater than 0 and less than 180)." << endl;
        }
    } while (newCustomFOVInDegrees <= 0 || newCustomFOVInDegrees >= 180);
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
            cout << "\nFailed to open " << filename << ", check if the file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently being used. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\n"
                 << filename " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

float NewHorizontalFOVInDegreesCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newHorizontalFOVInDegrees = 2.0f * RadToDeg(atan((static_cast<float>(newWidthValue) / static_cast<float>(newHeightValue)) / oldAspectRatio) * tan(DegToRad(oldHorizontalFOV / 2.0f)));
    return newHorizontalFOVInDegrees;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "Alcatraz: Prison Escape (2001) FOV Fixer v1.3 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file, "lithtech.exe");

        file.seekg(kHorizontalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentHorizontalFOVInRadians), sizeof(currentHorizontalFOVInRadians));

        file.seekg(kVerticalFOVOffset);
        file.read(reinterpret_cast<char *>(&currentVerticalFOVInRadians), sizeof(currentVerticalFOVInRadians));

        file.close();

        // Converts the field of view values from radians to degrees
        currentHorizontalFOVInDegrees = RadToDeg(currentHorizontalFOVInRadians);
        currentVerticalFOVInDegrees = RadToDeg(currentVerticalFOVInRadians);

        cout << "\nCurrent field of view: " << currentHorizontalFOVInDegrees << "\u00B0 (Horizontal); " << currentVerticalFOVInDegrees << "\u00B0 (Vertical)\n";

        cout << "\n- Do you want to set field of view automatically based on the desired resolution (1) or set custom horizontal and vertical field of view values (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            HandleResolutionInput(newWidth);

            cout << "\n- Enter the desired height: ";
            HandleResolutionInput(newHeight);

            // Calculates the new horizontal field of view
            newHorizontalFOVInDegrees = NewHorizontalFOVInDegreesCalculation(newWidth, newHeight);

            newHorizontalFOVInRadians = DegToRad(newHorizontalFOVInDegrees); // Converts degrees to radians

            newVerticalFOVInRadians = kDefaultVerticalFOVInRadians;

            newVerticalFOVInDegrees = RadToDeg(newVerticalFOVInRadians);

            descriptor = "automatically";

            break;

        case 2:
            cout << "\n- Enter the desired horizontal field of view (in degrees, default for 4:3 aspect ratio is 90\u00B0): ";
            HandleFOVInput(newHorizontalFOVInDegrees);

            cout << "\n- Enter the desired vertical field of view (in degrees, default for 4:3 aspect ratio is 67.5\u00B0): ";
            HandleFOVInput(newVerticalFOVInDegrees);

            newHorizontalFOVInRadians = DegToRad(newHorizontalFOVInDegrees); // Converts degrees to radians

            newVerticalFOVInRadians = DegToRad(newVerticalFOVInDegrees); // Converts degrees to radians

            descriptor = "manually";

            break;
        }

        OpenFile(file, "lithtech.exe");

        file.seekp(kHorizontalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newHorizontalFOVInRadians), sizeof(newHorizontalFOVInRadians));

        file.seekp(kVerticalFOVOffset);
        file.write(reinterpret_cast<const char *>(&newVerticalFOVInRadians), sizeof(newVerticalFOVInRadians));

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed " << descriptor << " the horizontal field of view to " << newHorizontalFOVInDegrees << "\u00B0 and vertical field of view to " << newVerticalFOVInDegrees << "\u00B0." << endl;
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
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        cout << "\n---------------------------\n";
    } while (choice2 == 2); // Checks the flag in the loop condition
}