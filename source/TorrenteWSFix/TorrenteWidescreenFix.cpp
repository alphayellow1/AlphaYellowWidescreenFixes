#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    cout << "Torrente (2001) Widescreen Fixer by AlphaYellow, 2024\n\n----------------\n";
    int16_t desiredWidth, desiredHeight;
    float fov;
    int choice, fileOpened;
    bool fileNotFound;

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("vtKernel.dll", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open vtKernel.dll, check if the file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently being used. Press Enter when all the mentioned problems are solved." << endl;
            getch(); // Waits for user input

            // Tries to open the file again
            file.open("vtKernel.dll", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nvtKernel.dll opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

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

        fov = ((4.0f / 3.0f) / (static_cast<float>(desiredWidth) / static_cast<float>(desiredHeight))) * 0.5f;

        file.seekp(0x00020914); // 640
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x0002092F); // 800
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x00020945); // 1024
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x0002091E); // 480
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x00020939); // 600
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x0002094F); // 768
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x00066254);
        file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

        cout << "\nSuccessfully changed the resolution and field of view." << endl;

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
                cout << " Invalid input. Please enter 1 to exit or 2 to try another value." << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << " Please enter a valid number." << endl;
            }
        } while (choice < 1 || choice > 2);
    } while (choice != 1); // Checks the flag in the loop condition

    cout << "\nPress Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}