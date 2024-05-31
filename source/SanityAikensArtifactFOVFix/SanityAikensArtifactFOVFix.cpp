#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <limits>
#include <conio.h>

using namespace std;

// Constants
const double kPi = 3.14159265358979323846;
const double kTolerance = 0.01;
const streampos kHFOVOffset = 0x0008AF74;
const streampos kVFOVOffset = 0x0008AF97;
const float kDefaultVFOVInRadians = 1.1806666851043701;

// Variables
int choice1, choice2, fileOpened, tempChoice;
bool fileNotFound, validKeyPressed;
double oldWidth = 4.0, oldHeight = 3.0, oldHFOV = 90.0, oldAspectRatio = oldWidth / oldHeight, newAspectRatio, newWidth, newHeight, currentHFOVInDegrees, currentVFOVInDegrees, newHFOVInDegrees, newVFOVInDegrees;
float currentHFOVInRadians, currentVFOVInRadians, newHFOVInRadians, newVFOVInRadians;
string descriptor, fovDescriptor;
fstream file;
char ch;

// Function to convert degrees to radians
double DegToRad(double degrees)
{
    return degrees * (kPi / 180.0);
}

// Function to convert radians to degrees
double RadToDeg(double radians)
{
    return radians * (180.0 / kPi);
}

// Function to handle user input in choices
void HandleUserInput(int &choice)
{
    tempChoice = -1;         // Temporary variable to store the input
    validKeyPressed = false; // Flag to track if a valid key was pressed

    while (true)
    {
        ch = _getch(); // Waits for user to press a key

        // Checks if the key is '1' or '2'
        if ((ch == '1' || ch == '2') && !validKeyPressed)
        {
            tempChoice = ch - '0';  // Stores the input temporarily
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
        else if (ch == '\r') // If 'Enter' is pressed
        {
            if (tempChoice != -1) // Checks if a valid input has been made
            {
                choice = tempChoice; // Assigns the temporary input to the choice variable
                cout << endl;        // Moves to a new line
                break;               // Exits the loop since we have a confirmed input
            }
        }
    }
}

// Function to open the file
void OpenFile(fstream &file)
{
    fileNotFound = false;
    fileOpened = 0; // Initializes fileOpened to 0

    file.open("client.dll", ios::in | ios::out | ios::binary);

    // If the file is not open, sets fileNotFound to true
    if (!file.is_open())
    {
        fileNotFound = true;
    }

    // Loops until the file is found and opened
    while (fileNotFound)
    {

        // Tries to open the file again
        file.open("client.dll", ios::in | ios::out | ios::binary);

        if (!file.is_open())
        {
            cout << "\nFailed to open client.dll, check if the DLL file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the DLL is currently being used. Press Enter when all the mentioned problems are solved." << endl;
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
        }
        else
        {
            cout << "\nclient.dll opened successfully!" << endl;
            fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
        }
    }
}

int main()
{
    cout << "Sanity: Aiken's Artifact (2000) FOV Fixer v1.4 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        OpenFile(file);

        file.seekg(kHFOVOffset);
        file.read(reinterpret_cast<char *>(&currentHFOVInRadians), sizeof(currentHFOVInRadians));
        file.seekg(kVFOVOffset);
        file.read(reinterpret_cast<char *>(&currentVFOVInRadians), sizeof(currentVFOVInRadians));

        file.close();

        // Converts the FOV values from radians to degrees
        currentHFOVInDegrees = RadToDeg(currentHFOVInRadians);
        currentVFOVInDegrees = RadToDeg(currentVFOVInRadians);

        // Checks if the FOV values correspond to exactly 90 (horizontal), and exactly or approximately 75 (vertical) degrees
        fovDescriptor = "";
        if (abs(currentHFOVInDegrees - 90.0) < numeric_limits<float>::epsilon() &&
            abs(currentVFOVInDegrees - 67.65) < kTolerance)
        {
            fovDescriptor = "[Default for 4:3 aspect ratio]";
        }

        cout << "\nYour current FOV: " << currentHFOVInDegrees << " degrees (Horizontal); " << currentVFOVInDegrees << " degrees (Vertical) " << fovDescriptor << "\n";

        cout << "\n- Do you want to set FOV automatically based on the desired resolution (1) or set custom horizontal and vertical FOV values (2)?: ";

        HandleUserInput(choice1);

        switch (choice1)
        {
        case 1:
            do
            {
                cout << "\n- Enter the desired width: ";
                cin >> newWidth;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    newWidth = -1;                                       // Ensures the loop continues
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (newWidth <= 0 || newWidth > 65535)
                {
                    cout << "Please enter a positive number for width less than 65536." << endl;
                }
            } while (newWidth <= 0 || newWidth > 65535);

            do
            {
                cout << "\n- Enter the desired height: ";
                cin >> newHeight;

                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    newHeight = -1;                                      // Ensures the loop continues
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (newHeight <= 0 || newHeight > 65535)
                {
                    cout << "Please enter a positive number for height less than 65536." << endl;
                }
            } while (newHeight <= 0 || newHeight > 65535);

            newAspectRatio = newWidth / newHeight;

            // Calculates the new horizontal FOV
            newHFOVInDegrees = 2.0 * RadToDeg(atan((newAspectRatio / oldAspectRatio) * tan(DegToRad(oldHFOV / 2.0))));

            newHFOVInRadians = static_cast<float>(DegToRad(newHFOVInDegrees)); // Converts degrees to radians

            newVFOVInRadians = kDefaultVFOVInRadians;

            newVFOVInDegrees = RadToDeg(newVFOVInRadians);

            descriptor = "automatically";

            break;

        case 2:
            do
            {
                cout << "\n- Enter the desired horizontal FOV (in degrees): ";
                cin >> newHFOVInDegrees;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    newHFOVInDegrees = -1;                               // Ensures the loop continues
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (newHFOVInDegrees <= 0 || newHFOVInDegrees >= 180)
                {
                    cout << "Please enter a valid number for the horizontal FOV." << endl;
                }
            } while (newHFOVInDegrees <= 0 || newHFOVInDegrees >= 180);

            do
            {
                cout << "\n- Enter the desired vertical FOV (in degrees): ";
                cin >> newVFOVInDegrees;

                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    newVFOVInDegrees = -1;                               // Ensures the loop continues
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (newVFOVInDegrees <= 0 || newVFOVInDegrees >= 180)
                {
                    cout << "Please enter a valid number for the vertical FOV." << endl;
                }
            } while (newVFOVInDegrees <= 0 || newVFOVInDegrees >= 180);

            newHFOVInRadians = static_cast<float>(DegToRad(newHFOVInDegrees)); // Converts degrees to radians

            newVFOVInRadians = static_cast<float>(DegToRad(newVFOVInDegrees)); // Converts degrees to radians

            descriptor = "manually";
        }

        OpenFile(file);

        file.seekp(kHFOVOffset);
        file.write(reinterpret_cast<const char *>(&newHFOVInRadians), sizeof(newHFOVInRadians));
        file.seekp(kVFOVOffset);
        file.write(reinterpret_cast<const char *>(&newVFOVInRadians), sizeof(newVFOVInRadians));

        // Confirmation message
        cout << "\nSuccessfully changed " << descriptor << " the horizontal FOV to " << newHFOVInDegrees << " degrees and vertical FOV to " << newVFOVInDegrees << " degrees."
             << endl;

        // Closes the file
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";

        HandleUserInput(choice2);

        if (choice2 == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Wait for user to press a key
            } while (ch != '\r'); // Keep waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }
    } while (choice2 == 2); // Checks the flag in the loop condition
}