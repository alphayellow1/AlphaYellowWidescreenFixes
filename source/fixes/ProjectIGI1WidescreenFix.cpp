#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint32_t variable type
#include <cmath>
#include <limits>
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>

using namespace std;

// Constants
const streampos kGameVersionCheckValue_Offset = 0x00000108;
const streampos kCameraFOV_USA_Offset = 0x00085321;
const streampos kCameraFOV_EU_CHINA_Offset = 0x00085271;
const streampos kCameraFOV_JP_Offset = 0x00084EB1;
const streampos kAspectRatio_USA_Offset = 0x0013358C;
const streampos kAspectRatio_EU_CHINA_Offset = 0x001335C8;
const streampos kAspectRatio_JP_Offset = 0x001335BC;
const streampos kWeaponModelFOV_USA_Offset = 0x001339BC;
const streampos kWeaponModelFOV_EU_CHINA_Offset = 0x001339C4;
const streampos kWeaponModelFOV_JP_Offset = 0x001339EC;

// Variables
int choice1, choice2, tempChoice;
int16_t GameVersionCheckValue;
uint32_t newWidth, newHeight;
bool fileNotFound, validKeyPressed;
float newAspectRatio, newAspectRatioValue, newCameraFOV, newCameraFOVValue, newWeaponModelFOV;
string input;
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

void HandleFOVInput(float &newCustomFOV)
{
    do
    {
        // Reads the input as a string
        cin >> input;

        // Replaces all commas with dots
        replace(input.begin(), input.end(), ',', '.');

        // Parses the string to a float
        newCustomFOV = stof(input);

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newCustomFOV <= 0)
        {
            cout << "Please enter a valid number for the FOV multiplier (greater than 0)." << endl;
        }
    } while (newCustomFOV <= 0);
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
            cout << "\nFailed to open " << filename << ", check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\n" << filename << " opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

void GameVersionCheck()
{
    do
    {
        OpenFile(file, "IGI.exe");

        file.seekg(kGameVersionCheckValue_Offset);
        file.read(reinterpret_cast<char *>(&GameVersionCheckValue), sizeof(GameVersionCheckValue));

        if (GameVersionCheckValue == 29894)
        {
            cout << "\nChinese / European version detected." << endl;
        }
        else if (GameVersionCheckValue == 19290)
        {
            cout << "\nAmerican version detected." << endl;
        }
        else if (GameVersionCheckValue == 1571)
        {
            cout << "\nJapanese version detected." << endl;
        }
        else
        {
            cout << "\nUnknown version detected. Please make sure to have either the Chinese/European, Japanese or American version present. Press Enter when the right version is present." << endl;

            file.close();

            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }

        file.close();

    } while (GameVersionCheckValue != 29894 && GameVersionCheckValue != 19290 && GameVersionCheckValue != 1571);
}

float NewAspectRatioCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newAspectRatioValue = (static_cast<float>(newWidth) / static_cast<float>(newHeight)) * 0.75f * 4.0f;
    return newAspectRatioValue;
}

float NewCameraFOVCalculation(uint32_t &newWidthValue, uint32_t &newHeightValue)
{
    newCameraFOVValue = (static_cast<float>(newWidth) / static_cast<float>(newHeight)) * 0.75f;
    return newCameraFOVValue;
}

int main()
{
    cout << "Project IGI 1 (2000) Widescreen Fixer v1.2 by AlphaYellow and AuToMaNiAk005, 2024\n";

    cout << "\n--------------------\n";

    GameVersionCheck();

    cout << "\n--------------------\n";

    do
    {
        OpenFile(file);

        cout << "\n- Enter the desired width: ";
        HandleResolutionInput(newWidth);

        cout << "\n- Enter the desired height: ";
        HandleResolutionInput(newHeight);

        newAspectRatio = NewAspectRatioCalculation(newWidth, newHeight);

        cout << "\n- Do you want to set camera FOV automatically based on the resolution typed above (1) or set a custom multiplier value for camera FOV (2)?: ";
        HandleChoiceInput(choice1);

        switch (choice1)
        {
        case 1:
            // Calculates the new camera FOV
            newCameraFOV = NewCameraFOVCalculation(newWidth, newHeight);
            break;

        case 2:
            cout << "\n- Enter the desired camera FOV (default for 4:3 aspect ratio is 1.0): ";
            HandleFOVInput(newCameraFOV);
            break;
        }

        cout << "\n- Enter the desired weapon FOV (default for 4:3 aspect ratio is 1.7559): ";
        HandleFOVInput(newWeaponModelFOV);

        if (GameVersionCheckValue == 29894) // CHINA/EU VERSIONS
        {
            file.seekp(kAspectRatio_EU_CHINA_Offset);
            file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

            file.seekp(kCameraFOV_EU_CHINA_Offset);
            file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

            file.seekp(kWeaponModelFOV_EU_CHINA_Offset);
            file.write(reinterpret_cast<const char *>(&newWeaponModelFOV), sizeof(newWeaponModelFOV));
        }
        else if (GameVersionCheckValue == 19290) // USA VERSION
        {
            file.seekp(kAspectRatio_USA_Offset);
            file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

            file.seekp(kCameraFOV_USA_Offset);
            file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

            file.seekp(kWeaponModelFOV_USA_Offset);
            file.write(reinterpret_cast<const char *>(&newWeaponModelFOV), sizeof(newWeaponModelFOV));
        }
        else if (GameVersionCheckValue == 1571) // JAPAN VERSION
        {
            file.seekp(kAspectRatio_JP_Offset);
            file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

            file.seekp(kCameraFOV_JP_Offset);
            file.write(reinterpret_cast<const char *>(&newCameraFOV), sizeof(newCameraFOV));

            file.seekp(kWeaponModelFOV_JP_Offset);
            file.write(reinterpret_cast<const char *>(&newWeaponModelFOV), sizeof(newWeaponModelFOV));
        }

        // Checks if any errors occurred during the file operations
        if (file.good())
        {
            // Confirmation message
            cout << "\nSuccessfully changed the aspect ratio and field of view." << endl;
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
    } while (choice2 == 2); // Checks the flag in the loop condition
}