#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <limits>

// Function to calculate the greatest common divisor
int gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
}

using namespace std;

int main()
{
    double newWidth, newHeight, fov2;
    float newAspectRatio, fov1;
    int choice, fileOpened, simplifiedWidth, simplifiedHeight, aspectRatioGCD;
    bool fileNotFound;

    cout << "The Lord of the Rings: The Fellowship of the Ring (2002) Widescreen Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("Fellowship.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open Fellowship.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            cin.get();

            // Tries to open the file again
            file.open("Fellowship.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nFellowship.exe opened successfully!" << endl;
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

        // Calculates the greatest common divisor of the width and height
        aspectRatioGCD = gcd(static_cast<int>(newWidth), static_cast<int>(newHeight));

        // Simplifies the width and height by dividing by the greatest common divisor
        simplifiedWidth = static_cast<int>(newWidth) / aspectRatioGCD;
        simplifiedHeight = static_cast<int>(newHeight) / aspectRatioGCD;

        if (simplifiedWidth == 16 && simplifiedHeight == 3)
        {
            simplifiedWidth = 48;
            simplifiedHeight = 9;
        }
        else if (simplifiedWidth == 8 && simplifiedHeight == 5)
        {
            simplifiedWidth = 16;
            simplifiedHeight = 10;
        }
        else if (simplifiedWidth == 7 && simplifiedHeight == 3)
        {
            simplifiedWidth = 21;
            simplifiedHeight = 9;
        }
        else if (simplifiedWidth == 5 && simplifiedHeight == 1)
        {
            simplifiedWidth = 45;
            simplifiedHeight = 9;
        }

        newAspectRatio = newWidth / newHeight;

        // Puts the aspect ratio value in the AR address
        file.seekp(0x0011D948);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        // Puts the FOV value in the FOV addresses (as float and double)
        fov1 = 64 / (newAspectRatio * 0.75);
        file.seekp(0x001208CC);
        file.write(reinterpret_cast<const char *>(&fov1), sizeof(fov1));
        fov2 = fov1;
        file.seekp(0x00120A90);
        file.write(reinterpret_cast<const char *>(&fov2), sizeof(fov2));

        // Confirmation message
        cout << "\nSuccessfully changed the aspect ratio to " << simplifiedWidth << ":" << simplifiedHeight << " and fixed the field of view. Now all " << simplifiedWidth << ":" << simplifiedHeight << " resolutions supported by the graphics card's driver will appear in the game's video settings." << endl;

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