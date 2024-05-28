#include <iostream>
#include <iomanip>
#include <fstream>
#include <limits>

using namespace std;

int main()
{
    float currentAspectRatio, newAspectRatio;
    int choice, fileOpened;
    int16_t desiredWidth, desiredHeight;
    bool fileNotFound;

    cout << "Harry Potter: Quidditch World Cup (2003) FOV Fixer v1.1 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("QWC.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open QWC.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if it's currently running. Press Enter when all the mentioned problems are solved." << endl;
            getch(); // Waits for user input

            // Tries to open the file again
            file.open("QWC.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nQWC.exe opened successfully!" << endl;
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
                cout << "Invalid input. Please enter a numeric value.\n"
                     << endl;
            }
            else if (desiredWidth <= 0 || desiredWidth > 65535)
            {
                cout << "Please enter a positive number for width less than 65536.\n"
                     << endl;
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
                cout << "Invalid input. Please enter a numeric value.\n"
                     << endl;
            }
            else if (desiredHeight <= 0 || desiredHeight > 65535)
            {
                cout << "Please enter a positive number for height less than 65536.\n"
                     << endl;
            }
        } while (desiredHeight <= 0 || desiredHeight > 65535);

        newAspectRatio = static_cast<float>(desiredWidth) / static_cast<float>(desiredHeight);

        file.seekp(0x0009F445);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        // Confirmation message
        cout << "\nSuccessfully changed the field of view."
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