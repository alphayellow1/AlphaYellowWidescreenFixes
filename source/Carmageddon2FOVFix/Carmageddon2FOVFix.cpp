#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>
#include <windows.h>

using namespace std;

int main()
{
    int choice, fileOpened;
    cout << "Carmageddon II: Carpocalypse Now (1998) Widescreen Fix by AlphaYellow, 2024\n\n----------------\n\n";
    float width, height, aspectratio, fov;
    bool fileNotFound, validInput = false;

    do
    {
        cout << "Enter the desired width: ";
        cin >> width;

        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        TCHAR buffer[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, buffer);

        // Tries to open the file initially
        file.open("CARMA2_HW0.EXE", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open CARMA2_HW0.EXE, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            cin.clear();                                         // Clear error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
            cin.get();                                           // Waits for the user to press a key

            // Tries to open the file again
            file.open("CARMA2_HW0.EXE", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nCARMA2_HW0.EXE opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

        if (cin.fail())
        {
            cin.clear();                                         // Clear error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignore invalid input
            width = -1;                                          // Ensure the loop continues
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (width <= 0 || width > 65535)
        {
            cout << "Please enter a positive number for width less than 65536." << endl;
        }
    } while (width <= 0 || width > 65535);

    do
    {
        cout << "\nEnter the desired height: ";
        cin >> height;

        if (cin.fail())
        {
            cin.clear();                                         // Clear error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignore invalid input
            height = -1;                                         // Ensure the loop continues
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (height <= 0 || height > 65535)
        {
            cout << "Please enter a positive number for height less than 65536." << endl;
        }
    } while (height <= 0 || height > 65535);

    fstream file("CARMA2_HW0.EXE", ios::in | ios::out | ios::binary);

    aspectratio = width / height;

    file.seekp(0x001874A3);
    file.write(reinterpret_cast<const char *>(&aspectratio), sizeof(aspectratio));

    cout << "\nSuccessfully changed the field of view." << endl;

    // Close the file
    file.close();

    cout << "\nPress Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}