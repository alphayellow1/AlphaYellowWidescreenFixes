#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h>
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
    double oldWidth = 4.0;
    double oldHeight = 3.0;
    double oldAspectRatio = oldWidth / oldHeight;
    double oldHorizontalFOV = 90.0; // Known horizontal FOV for 4:3 aspect ratio
    double newHorizontalFOV;
    double newAspectRatio;
    double newWidth, newHeight, horizontalFOV;
    float horizontalFovInRadians;

    cout << "Nina: Agent Chronicles (2002) FOV Fixer by AlphaYellow, 2024\n\n----------------\n\n";

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

    fstream file("lithtech.exe", ios::in | ios::out | ios::binary);
    if (!file.is_open())
    {
        cout << "Failed to open the file." << endl;
        return 1;
    }

    horizontalFovInRadians = static_cast<float>(newHorizontalFOV * (M_PI / 180.0)); // Converts degrees to radians

    file.seekp(0x000CAF91);
    file.write(reinterpret_cast<const char *>(&horizontalFovInRadians), sizeof(horizontalFovInRadians));

    // Confirmation message
    cout << "\nSuccessfully fixed the field view in the executable.\n"
         << endl;

    // Closes the file
    file.close();

    cout << "Press enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}