#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <limits>

// Define M_PI if not already defined
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
    int choice, fileOpened;
    bool fileNotFound;
    double oldWidth = 4.0, oldHeight = 3.0, oldAspectRatio = oldWidth / oldHeight, oldHorizontalFOV = 90.0, newHorizontalFOV, newAspectRatio, newWidth, newHeight, horizontalFOV;
    float horizontalFovInRadians;

    cout << "Nina: Agent Chronicles (2002) FOV Fixer v1.3 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("lithtech.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open lithtech.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            cin.get();

            // Tries to open the file again
            file.open("lithtech.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nlithtech.exe opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

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
        newHorizontalFOV = 2.0 * radToDeg(atan((newAspectRatio / oldAspectRatio) * tan(degToRad(oldHorizontalFOV / 2.0))));

        horizontalFovInRadians = static_cast<float>(newHorizontalFOV * (M_PI / 180.0)); // Converts degrees to radians

        file.seekp(0x000CAF91);
        file.write(reinterpret_cast<const char *>(&horizontalFovInRadians), sizeof(horizontalFovInRadians));

        // Confirmation message
        cout << "\nSuccessfully fixed the field of view."
             << endl;

        // Closes the file
        file.close();

        do
        {
            cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
            cin >> choice;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice = -1;
                cout << "Invalid input. Please enter 1 to exit the program or 2 to try another value."
                     << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << "Please enter a valid number."
                     << endl;
            }
        } while (choice < 1 || choice > 2);
    } while (choice == 2); // Checks the flag in the loop condition

    cout << "\nPress enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}