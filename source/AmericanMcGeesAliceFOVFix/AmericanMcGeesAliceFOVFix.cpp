#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h>
#include <cstdint>
#include <cmath>
#include <limits>
#include <windows.h>

// Defines M_PI if not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Function to convert degrees to radians
double degToRad(double degrees)
{
    return degrees * (M_PI / 180.0);
}

// Function to convert radians to degrees
double radToDeg(double radians)
{
    return radians * (180.0 / M_PI);
}

using namespace std;

int main()
{
    int choice1, choice2, choice3;
    float fov;
    bool fileNotFound = false;
    double newWidth, newHeight;
    double oldWidth = 4.0;
    double oldHeight = 3.0;
    double oldAspectRatio = oldWidth / oldHeight;
    double oldHorizontalFOV = 90.0; // Known horizontal FOV for 4:3 aspect ratio
    double newHorizontalFOV;
    double newAspectRatio;

    SetConsoleOutputCP(CP_UTF8);

    cout << " American McGee's Alice (2000) FOV Fixer by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        do
        {
            cout << "\n - Do you want to set FOV automatically based on the desired resolution (1) or set a custom FOV value (2) ?: ";
            cin >> choice1;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice1 = -1;                                        // Ensures the loop continues
                cout << " Invalid input. Please enter a numeric value." << endl;
            }
            else if (choice1 < 1 || choice1 > 2)
            {
                cout << " Please enter a valid number."
                     << endl;
            }
        } while (choice1 < 1 || choice1 > 2);

        if (choice1 == 1)
        {
            do
            {
                cout << "\n - Enter the desired width: ";
                cin >> newWidth;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    newWidth = -1;                                       // Ensures the loop continues
                    cout << " Invalid input. Please enter a numeric value." << endl;
                }
                else if (newWidth <= 0 || newWidth > 65535)
                {
                    cout << " Please enter a positive number for width less than 65536." << endl;
                }
            } while (newWidth <= 0 || newWidth > 65535);

            do
            {
                cout << "\n - Enter the desired height: ";
                cin >> newHeight;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    newHeight = -1;                                      // Ensures the loop continues
                    cout << " Invalid input. Please enter a numeric value." << endl;
                }
                else if (newHeight <= 0 || newHeight > 65535)
                {
                    cout << " Please enter a positive number for height less than 65536." << endl;
                }
            } while (newHeight <= 0 || newHeight > 65535);

            newAspectRatio = newWidth / newHeight;

            // Calculates the new horizontal FOV
            fov = 2.0 * radToDeg(atan((newAspectRatio / oldAspectRatio) * tan(degToRad(oldHorizontalFOV / 2.0))));

            fstream file("Base/fgamex86.dll", ios::in | ios::out | ios::binary);
            if (!file.is_open())
            {
                cout << " Failed to open the file." << endl;
                fileNotFound = true; // Sets the flag to true as the file is not found
                break;               // Breaks out of the loop
            }

            file.seekp(0x000A558E);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            file.seekp(0x0006CACF);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

            // Confirmation message
            cout << "\n Successfully changed the field of view in the executable."
                 << endl;

            // Closes the file
            file.close();

            do
            {
                cout << "\n - Do you want to exit the program (1) or try another value (2) ?: ";
                cin >> choice2;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    choice2 = -1;
                    cout << " Invalid input. Please enter 1 to exit or 2 to try another value.\n"
                         << endl;
                }
                else if (choice2 < 1 || choice2 > 2)
                {
                    cout << " Please enter a valid number." << endl;
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
                }
            } while (choice2 < 1 || choice2 > 2);
        }
        else
        {
            do
            {
                cout << "\n - Enter the desired FOV (from 1\u00B0 to 180\u00B0): ";
                cin >> fov;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    cout << " Invalid input. Please enter a numeric value." << endl;
                }
                else if (fov < 1 || fov > 180)
                {
                    cout << " Please enter a valid number for the FOV." << endl;
                }
            } while (fov < 1 || fov > 180);

            fstream file("Base/fgamex86.dll", ios::in | ios::out | ios::binary);
            if (!file.is_open())
            {
                cout << "\n Failed to open the file. Please check the file path and permissions." << endl;
                fileNotFound = true; // Set the flag to true as the file is not found
                break;               // Break out of the loop
            }

            file.seekp(0x000A558E);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            file.seekp(0x0006CACF);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

            // Confirmation message
            cout << "\n Successfully changed the field of view to " << fov << "\u00B0" << " in the executable.\n"
                 << endl;

            // Closes the file
            file.close();

            do
            {
                cout << " - Do you want to exit the program (1) or try another value (2) ?: ";
                cin >> choice3;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    choice3 = -1;
                    cout << " Invalid input. Please enter 1 to exit or 2 to try another value." << endl;
                }
                else if (choice3 < 1 || choice3 > 2)
                {
                    cout << " Please enter a valid number." << endl;
                }
            } while (choice3 < 1 || choice3 > 2);
        }
    } while (choice2 == 2 || choice3 == 2 && !fileNotFound); // Checks the flag in the loop condition

    cout << "\n Press Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}

== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <limits>
#include <windows.h>
#include <conio.h>
#include <string>
#include <algorithm>

    using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const streampos kFOVOffset1 = 0x000A558E;
const streampos kFOVOffset2 = 0x0006CACF;

// Variables
int choice1, choice2, fileOpened, tempChoice;
bool fileNotFound, validKeyPressed;
double oldWidth = 4.0, oldHeight = 3.0, oldHFOV = 90.0, oldAspectRatio = oldWidth / oldHeight, newAspectRatio, newWidth, newHeight, currentHFOVInDegrees, currentVFOVInDegrees, newHFOVInDegrees, newVFOVInDegrees, newCustomFOVInDegrees, newCustomResolutionValue, newFOV;
float currentFOV, fovInFloat;
string descriptor, fovDescriptor, input;
fstream file;
char ch;

// Function to convert degrees to radians
double degToRad(double degrees)
{
    return degrees * (kPi / 180.0);
}

// Function to convert radians to degrees
double radToDeg(double radians)
{
    return radians * (180.0 / kPi);
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

float HandleFOVInput()
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
            cout << "Please enter a valid number for the FOV (greater than 0 and less than 180)." << endl;
        }
    } while (newCustomFOVInDegrees <= 0 || newCustomFOVInDegrees >= 180);

    return newCustomFOVInDegrees;
}

double HandleResolutionInput()
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

    file.open("Base/fgamex86.dll", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {

        // Tries to open the file again
        file.open("Base/fgamex86.dll", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open fgamex86.dll, check if the file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently being used. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nfgamex86.dll opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    cout << "American McGee's Alice (2000) FOV Fixer v1.0 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kFOVOffset1);
        file.read(reinterpret_cast<char *>(&currentFOV), sizeof(currentFOV));
        file.seekg(kFOVOffset2);
        file.read(reinterpret_cast<char *>(&currentFOV), sizeof(currentFOV));

        cout << "\nThe current FOV is " << currentFOV << "\u00B0" << endl;

        cout << "\n- Do you want to set FOV automatically based on the desired resolution (1) or set a custom FOV value (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            cout << "\n- Enter the desired width: ";
            newWidth = HandleResolutionInput();

            cout << "\n- Enter the desired height: ";
            newHeight = HandleResolutionInput();

            newAspectRatio = newWidth / newHeight;

            // Calculates the new FOV
            newFOV = 2.0 * radToDeg(atan((newAspectRatio / oldAspectRatio) * tan(degToRad(oldHFOV / 2.0))));

            break;

        case 2:
            cout << "\n- Enter the desired FOV value (from 1\u00B0 to 180\u00B0, default FOV for 4:3 aspect ratio is 90.0\u00B0): ";
            newFOV = HandleFOVInput();

            break;
        }

        fovInFloat = static_cast<float>(newFOV);

        file.seekp(kFOVOffset1);
        file.write(reinterpret_cast<const char *>(&fovInFloat), sizeof(fovInFloat));
        file.seekp(kFOVOffset2);
        file.write(reinterpret_cast<const char *>(&fovInFloat), sizeof(fovInFloat));

        // Confirmation message
        cout << "\nSuccessfully changed the FOV to " << fovInFloat << "\u00B0." << endl;

        // Closes the file
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice2);

        if (choice2 == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }
    } while (choice2 != 1); // Checks the flag in the loop condition
}