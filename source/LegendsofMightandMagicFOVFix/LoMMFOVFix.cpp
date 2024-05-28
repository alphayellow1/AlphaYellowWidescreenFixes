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
    double oldWidth = 4.0, oldHeight = 3.0, oldHorizontalFOV = 90.0, newHorizontalFOV, newAspectRatio, newWidth, newHeight, result, horizontalFOV, verticalFOV, oldAspectRatio;
    float horizontalFovInRadians1, verticalFovInRadians1, horizontalFovInRadians2, verticalFovInRadians2, currentHFOV, currentVFOV, newHFOV1, newVFOV1, newHFOV2, newVFOV2;

    cout << "Legends of Might and Magic (2001) FOV Fixer v1.3 by AlphaYellow, 2024\n\n----------------\n";

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

        file.seekg(0x0007CB01);
        file.read(reinterpret_cast<char *>(&currentHFOV), sizeof(currentHFOV));
        file.seekg(0x0007CB26);
        file.read(reinterpret_cast<char *>(&currentVFOV), sizeof(currentVFOV));

        // Defines a tolerance for the approximation
        const double tolerance = 0.01;

        // Converts the FOV values from radians to degrees
        currentHFOV = radToDeg(currentHFOV);
        currentVFOV = radToDeg(currentVFOV);

        // Checks if the FOV values correspond to exactly 90 (horizontal), and exactly or approximately 75 (vertical) degrees
        string fovDescriptor = "";
        if (abs(currentHFOV - 90.0) < numeric_limits<float>::epsilon() &&
            abs(currentVFOV - 67.5) < tolerance)
        {
            fovDescriptor = "[Default for 4:3 aspect ratio]";
        }

        cout << "\n- Your current FOV: " << currentHFOV << " degrees (Horizontal); " << currentVFOV << " degrees (Vertical) " << fovDescriptor << "\n\n";

        do
        {
            cout << "- Do you want to set FOV automatically based on the desired resolution (1) or set custom horizontal and vertical FOV values (2)?: ";
            cin >> choice;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice = -1;                                         // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value.\n"
                     << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << "Please enter a valid number.\n"
                     << endl;
            }
        } while (choice < 1 || choice > 2);

        switch (choice)
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

            oldAspectRatio = oldWidth / oldHeight;

            newAspectRatio = newWidth / newHeight;

            // Calculates the new horizontal FOV
            newHorizontalFOV = 2.0 * radToDeg(atan((newAspectRatio / oldAspectRatio) * tan(degToRad(oldHorizontalFOV / 2.0))));

            horizontalFovInRadians1 = static_cast<float>(newHorizontalFOV * (M_PI / 180.0)); // Converts degrees to radians

            verticalFovInRadians1 = 1.1780972480773926;

            file.seekp(0x0007CB01);
            file.write(reinterpret_cast<const char *>(&horizontalFovInRadians1), sizeof(horizontalFovInRadians1));
            file.seekp(0x0007CB26);
            file.write(reinterpret_cast<const char *>(&verticalFovInRadians1), sizeof(verticalFovInRadians1));

            newHFOV1 = radToDeg(horizontalFovInRadians1);
            newVFOV1 = radToDeg(verticalFovInRadians1);

            // Confirmation message
            cout << "\nSuccessfully changed automatically the horizontal FOV to " << newHFOV1 << " degrees and vertical FOV to " << newVFOV1 << " degrees."
                 << endl;

            // Closes the file
            file.close();
            break;

        case 2:
            do
            {
                cout << "\n- Enter the desired horizontal FOV (in degrees): ";
                cin >> horizontalFOV;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    horizontalFOV = -1;                                  // Ensures the loop continues
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (horizontalFOV <= 0 || horizontalFOV >= 180)
                {
                    cout << "Please enter a valid number for the horizontal FOV." << endl;
                }
            } while (horizontalFOV <= 0 || horizontalFOV >= 180);

            do
            {
                cout << "\n- Enter the desired vertical FOV (in degrees): ";
                cin >> verticalFOV;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    verticalFOV = -1;                                    // Ensures the loop continues
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (verticalFOV <= 0 || verticalFOV >= 180)
                {
                    cout << "Please enter a valid number for the vertical FOV." << endl;
                }
            } while (verticalFOV <= 0 || verticalFOV >= 180);

            horizontalFovInRadians2 = static_cast<float>(horizontalFOV * (M_PI / 180.0)); // Converts degrees to radians
            verticalFovInRadians2 = static_cast<float>(verticalFOV * (M_PI / 180.0));     // Converts degrees to radians

            file.seekp(0x0007CB01);
            file.write(reinterpret_cast<const char *>(&horizontalFovInRadians2), sizeof(horizontalFovInRadians2));
            file.seekp(0x0007CB26);
            file.write(reinterpret_cast<const char *>(&verticalFovInRadians2), sizeof(verticalFovInRadians2));

            newHFOV2 = radToDeg(horizontalFovInRadians2);
            newVFOV2 = radToDeg(verticalFovInRadians2);

            // Confirmation message
            cout << "\nSuccessfully changed manually the horizontal FOV to " << newHFOV2 << " degrees and vertical FOV to " << newVFOV2 << " degrees.\n"
                 << endl;

            // Closes the file
            file.close();
            break;
        }

        do
        {
            cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
            cin >> choice;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice = -1;
                cout << "Invalid input. Please enter 1 to exit the program or 2 to try another value.\n"
                     << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << "Please enter a valid number.\n"
                     << endl;
            }
        } while (choice < 1 || choice > 2);
    } while (choice == 2); // Checks the flag in the loop condition

    cout << "\nPress enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}