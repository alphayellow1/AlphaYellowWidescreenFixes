#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <limits>

using namespace std;

int main()
{
    int choice, fileOpened;
    int16_t width, height;
    float fov;
    bool fileNotFound;

    cout << "Jacked (2006) Widescreen Fixer v1.1 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("Jacked.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open Jacked.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            cin.get();

            // Tries to open the file again
            file.open("Jacked.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nJacked.exe opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

        do
        {
            cout << "\n- Enter the desired width: ";
            cin >> width;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                width = -1;                                          // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value." << endl;
            }
            else if (width <= 0 || width > 65535)
            {
                cout << "Please enter a positive number for width less than 65536." << endl;
            }
        } while (width <= 0 || width > 65535);

        do
        {
            cout << "\n- Enter the desired height: ";
            cin >> height;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                height = -1;                                         // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value." << endl;
            }
            else if (height <= 0 || height > 65535)
            {
                cout << "Please enter a positive number for height less than 65536." << endl;
            }
        } while (height <= 0 || height > 65535);

        file.seekp(0x0007D27D);
        file.write(reinterpret_cast<const char *>(&width), sizeof(width));

        file.seekp(0x0007D286);
        file.write(reinterpret_cast<const char *>(&height), sizeof(height));

        fov = (4.0f / 3.0f) / (static_cast<float>(width) / static_cast<float>(height));
        file.seekp(0x0023F26C);
        file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

        // Confirmation message
        cout << "\nSuccessfully changed resolution to " << width << "x" << height << " and fixed the field of view." << endl;

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
                cout << "Invalid input. Please enter 1 to exit or 2 to try another value.\n"
                     << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << "Please enter a valid number.\n"
                     << endl;
            }
        } while (choice < 1 || choice > 2);

    } while (choice != 1); // Checks the flag in the loop condition

    cout << "\nPress Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}