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
const streampos kAspectRatioOffset = 0x000115E9;
const streampos kFOVOffset = 0x0009C206;

// Variables
float newAspectRatio, FOV;
bool fileNotFound, aspectRatioWrittenToFile, isFOVKnown, validKeyPressed;
int choice, choice2, fileOpened, tempChoice;
double newCustomResolutionValue, newWidth, newHeight;
fstream file;
string input;
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

    file.open("aladdin.exe", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {

        // Tries to open the file again
        file.open("aladdin.exe", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open aladdin.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\naladdin.exe opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{

    cout << "Aladdin in Nasira's Revenge (2001) Widescreen Fixer v1.3 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        aspectRatioWrittenToFile = false;
        isFOVKnown = false;

        cout << "\n- Enter the desired width: ";
        newWidth = HandleResolutionInput();

        cout << "\n- Enter the desired height: ";
        newHeight = HandleResolutionInput();

        newAspectRatio = static_cast<float>(((4.0f / 3.0f) / (newWidth / newHeight)) * 0.75f);

        file.seekp(kAspectRatioOffset);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        aspectRatioWrittenToFile = true;

        cout << "\n- Do you want to fix the FOV automatically based on the resolution typed above (1) or set a custom FOV multiplier value (2)?: ";
        HandleChoiceInput(choice);

        switch (choice)
        {
        case 1:
            if (newAspectRatio == 0.75)
            {
                FOV = 0.8546222448; // 4:3
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.5625)
            {
                FOV = 1.139; // 16:9
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.625)
            {
                FOV = 1.025; // 16:10
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.8)
            {
                FOV = 0.8; // 5:4
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.6)
            {
                FOV = 1.0674; // 15:9
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.421875)
            {
                FOV = 1.518; // 21:9 (2560x1080)
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.41860465116)
            {
                FOV = 1.53; // 21:9 (3440x1440)
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.4166666)
            {
                FOV = 1.54; // 21:9 (3840x1600)
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.3125)
            {
                FOV = 2.05; // 32:10
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.28125)
            {
                FOV = 2.28; // 32:9
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.2666666)
            {
                FOV = 2.4; // 15:4
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.25)
            {
                FOV = 2.565; // 12:3
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.2083333)
            {
                FOV = 3.076; // 48:10
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.2)
            {
                FOV = 3.205; // 45:9
                isFOVKnown = true;
            }
            else if (newAspectRatio == 0.1875)
            {
                FOV = 3.42; // 48:9
                isFOVKnown = true;
            }
            else
            {
                cout << "\nThe FOV on this aspect ratio can't yet be fixed with this patch, please contact AlphaYellow on the mod's page or through Discord (alphayellow) to add support for it or set a custom FOV multiplier value." << endl;
                isFOVKnown = false;
            }
            break;

        case 2:
            cout << "\n- Type a custom FOV multiplier value (default for 4:3 aspect ratio is 0,8546222448): ";

            // Reads the input as a string
            cin >> input;

            // Replaces all commas with dots
            replace(input.begin(), input.end(), ',', '.');

            // Parses the string to a float
            FOV = stof(input);

            isFOVKnown = true;
            break;
        }

        if (isFOVKnown = true)
        {
            file.seekp(kFOVOffset);
            file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
        }

        // Confirmation message
        if (aspectRatioWrittenToFile && !isFOVKnown)
        {
            cout << "\nSuccessfully changed the aspect ratio." << endl;
        }
        else if (aspectRatioWrittenToFile && isFOVKnown)
        {
            cout << "\nSuccessfully changed the aspect ratio and field of view." << endl;
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
    } while (choice2 == 2); // Checks the flag in the loop condition
}