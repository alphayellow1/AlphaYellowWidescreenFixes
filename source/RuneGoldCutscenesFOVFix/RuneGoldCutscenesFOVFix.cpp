#include <iostream>
#include <iomanip>
#include <fstream>
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
    int choice1, choice2, fileOpened;
    bool fileNotFound;
    float newFOV, currentFOV;
    double newWidth, newHeight, oldWidth = 4.0, oldHeight = 3.0, oldAspectRatio = oldWidth / oldHeight, oldHorizontalFOV = 75.0, newHorizontalFOV, newAspectRatio;

    SetConsoleOutputCP(CP_UTF8);

    cout << "Rune Gold (2001) Cutscenes FOV Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("Engine.u", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open Engine.u, check if the file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the file is currently being used. Press Enter when all the mentioned problems are solved." << endl;
            cin.get();

            // Tries to open the file again
            file.open("Engine.u", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nEngine.u opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

        file.seekg(0x000132FB);
        file.read(reinterpret_cast<char *>(&currentFOV), sizeof(currentFOV));

        cout << "\nThe current cutscenes FOV is " << currentFOV << "\u00B0" << endl;

        do
        {
            cout << "\n- Do you want to set cutscenes FOV automatically based on the desired resolution (1) or set a custom cutscenes FOV value (2)?: ";
            cin >> choice1;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice1 = -1;                                        // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value." << endl;
            }
            else if (choice1 < 1 || choice1 > 2)
            {
                cout << "Please enter a valid number."
                     << endl;
            }
        } while (choice1 < 1 || choice1 > 2);

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

            // Calculates the new FOV
            newFOV = 2.0 * radToDeg(atan((newAspectRatio / oldAspectRatio) * tan(degToRad(oldHorizontalFOV / 2.0))));

            break;

        case 2:
            do
            {
                cout << "\n- Enter the desired cutscenes FOV value (from 1\u00B0 to 180\u00B0, default FOV for 4:3 aspect ratio is 75.0\u00B0): ";
                cin >> newFOV;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (newFOV < 1 || newFOV > 180)
                {
                    cout << "Please enter a valid cutscenes FOV value." << endl;
                }
            } while (newFOV < 1 || newFOV > 180);

            break;
        }

        // Searches for the 000132FB hexadecimal address in Engine.u and writes the new FOV value (4-bytes floating point number) into it
        file.seekp(0x000132FB);
        file.write(reinterpret_cast<const char *>(&newFOV), sizeof(newFOV));

        // Confirmation message
        cout << "\nSuccessfully changed the cutscenes FOV to " << newFOV << "\u00B0" << "."
             << endl;

        // Closes the file
        file.close();

        do
        {
            cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
            cin >> choice2;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice2 = -1;
                cout << "Invalid input. Please enter 1 to exit or 2 to try another value."
                     << endl;
            }
            else if (choice2 < 1 || choice2 > 2)
            {
                cout << "Please enter a valid number." << endl;
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
            }
        } while (choice2 < 1 || choice2 > 2);
    } while (choice2 != 1); // Checks the flag in the loop condition

    cout << "\nPress Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}