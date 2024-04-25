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
    float fov, fov2;
    bool fileNotFound = false;
    double newWidth, newHeight;
    double oldWidth = 4.0;
    double oldHeight = 3.0;
    double oldAspectRatio = oldWidth / oldHeight;
    double oldHorizontalFOV = 75.0; // Known horizontal FOV for 4:3 aspect ratio
    double newHorizontalFOV;
    double newAspectRatio;

    SetConsoleOutputCP(CP_UTF8);

    cout << "Rune Gold (2001) Cutscenes FOV Fixer by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file("Engine.u", ios::in | ios::out | ios::binary);
        if (!file.is_open())
        {
            cout << "Failed to open the file." << endl;
            return 1;
        }

        file.seekg(0x000132FB);
        file.read(reinterpret_cast<char *>(&fov2), sizeof(fov2));

        cout << "\nThe current cutscenes FOV is " << fov2 << "\u00B0" << endl;

        do
        {
            cout << "\n- Do you want to set cutscenes FOV automatically based on the desired resolution (1) or set a custom cutscenes FOV value (2) ?: ";
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

            // Calculates the new horizontal FOV
            fov = 2.0 * radToDeg(atan((newAspectRatio / oldAspectRatio) * tan(degToRad(oldHorizontalFOV / 2.0))));

            if (!file.is_open())
            {
                cout << "Failed to open the file." << endl;
                fileNotFound = true; // Sets the flag to true as the file is not found
                break;               // Breaks out of the loop
            }

            file.seekp(0x000132FB);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

            // Confirmation message
            cout << "\nSuccessfully changed the cutscenes FOV automatically to " << fov << "\u00B0."
                 << endl;

            // Closes the file
            file.close();

            do
            {
                cout << "\n- Do you want to exit the program (1) or try another value (2) ?: ";
                cin >> choice2;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    choice2 = -1;
                    cout << "Invalid input. Please enter 1 to exit or 2 to try another value.\n"
                         << endl;
                }
                else if (choice2 < 1 || choice2 > 2)
                {
                    cout << "Please enter a valid number." << endl;
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
                }
            } while (choice2 < 1 || choice2 > 2);

            break;

        case 2:
            do
            {
                cout << "\n- Enter the desired cutscenes FOV value (from 1\u00B0 to 180\u00B0, default FOV for 4:3 aspect ratio is 75.0\u00B0): ";
                cin >> fov;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (fov < 1 || fov > 180)
                {
                    cout << "Please enter a valid cutscenes FOV value." << endl;
                }
            } while (fov < 1 || fov > 180);

            if (!file.is_open())
            {
                cout << "\nFailed to open the file. Please check the file path and permissions." << endl;
                fileNotFound = true; // Set the flag to true as the file is not found
                break;               // Break out of the loop
            }

            file.seekp(0x000132FB);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

            // Confirmation message
            cout << "\nSuccessfully changed the cutscenes FOV to " << fov << "\u00B0" << ".\n"
                 << endl;

            // Closes the file
            file.close();

            do
            {
                cout << "- Do you want to exit the program (1) or try another value (2) ?: ";
                cin >> choice3;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    choice3 = -1;
                    cout << "Invalid input. Please enter 1 to exit or 2 to try another value." << endl;
                }
                else if (choice3 < 1 || choice3 > 2)
                {
                    cout << "Please enter a valid number." << endl;
                }
            } while (choice3 < 1 || choice3 > 2);
            break;
        }

    } while (choice2 != 1 && choice3 != 1 && !fileNotFound); // Checks the flag in the loop condition

    cout << "\nPress Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}
