#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    float hFOV, clippingFix;
    int16_t currentWidth, currentHeight, desiredWidth, desiredHeight;
    int choice, fileOpened;
    bool fileNotFound;

    cout << "State of Emergency (2003) Widescreen Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("KaosPC.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open KaosPC.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            cin.get();

            // Tries to open the file again
            file.open("KaosPC.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nKaosPC.exe opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

        file.seekg(0x000B77C1);
        file.read(reinterpret_cast<char *>(&currentWidth), sizeof(currentWidth));
        file.seekg(0x000B77D1);
        file.read(reinterpret_cast<char *>(&currentHeight), sizeof(currentHeight));

        cout << "\nYour current resolution is " << currentWidth << "x" << currentHeight << "." << endl;

        do
        {
            cout << "\n- Enter the desired width: ";
            cin >> desiredWidth;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                desiredWidth = -1;                                   // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value." << endl;
            }
            else if (desiredWidth <= 0 || desiredWidth > 65535)
            {
                cout << "Please enter a positive number for width less than 65536." << endl;
            }
        } while (desiredWidth <= 0 || desiredWidth > 65535);

        do
        {
            cout << "\n- Enter the desired height: ";
            cin >> desiredHeight;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                desiredHeight = -1;                                  // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value." << endl;
            }
            else if (desiredHeight <= 0 || desiredHeight > 65535)
            {
                cout << "Please enter a positive number for height less than 65536." << endl;
            }
        } while (desiredHeight <= 0 || desiredHeight > 65535);

        hFOV = desiredWidth / desiredHeight;
        clippingFix = (4.0f / 3.0f) / (desiredWidth / desiredHeight);

        file.seekp(0x000B77C1);
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x000B77D1);
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x00086057);
        file.write(reinterpret_cast<const char *>(&hFOV), sizeof(hFOV));
        file.seekp(0x0015F6E0);
        file.write(reinterpret_cast<const char *>(&clippingFix), sizeof(clippingFix));

        // Confirmation message
        cout << "\nSuccessfully changed the resolution to " << desiredWidth << "x" << desiredHeight << "." << endl;

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
                cout << "Invalid input. Please enter 1 to exit or 2 to try another value." << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << "Please enter a valid number." << endl;
            }
        } while (choice < 1 || choice > 2);
    } while (choice == 2); // Checks the flag in the loop condition

    cout << "\nPress Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}