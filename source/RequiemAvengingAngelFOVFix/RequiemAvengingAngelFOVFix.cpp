#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    float width, height, desiredFOV;
    int choice, fileOpened;
    bool fileNotFound;

    cout << "Requiem: Avenging Angel (1999) FOV Fixer v1.2 by AlphaYellow, 2024\n\n----------------\n";

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("D3D.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open D3D.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            cin.get();

            // Tries to open the file again
            file.open("D3D.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nD3D.exe opened successfully!" << endl;
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
                cin.clear();                                                   // Clear error flags
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
                width = -1;                                                    // Ensure the loop continues
                cout << "Invalid input. Please enter a numeric value." << std::endl;
            }
            else if (width <= 0 || width > 65535)
            {
                cout << "Please enter a positive number for width less than 65536." << std::endl;
            }
        } while (width <= 0 || width > 65535);

        do
        {
            cout << "\n- Enter the desired height: ";
            cin >> height;

            if (cin.fail())
            {
                cin.clear();                                                   // Clear error flags
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
                height = -1;                                                   // Ensure the loop continues
                cout << "Invalid input. Please enter a numeric value." << endl;
            }
            else if (height <= 0 || height > 65535)
            {
                std::cout << "Please enter a positive number for height less than 65536." << endl;
            }
        } while (height <= 0 || height > 65535);

        desiredFOV = (4.0f / 3.0f) / (width / height);

        file.seekp(0x000242E6); // HFOV
        file.write(reinterpret_cast<const char *>(&desiredFOV), sizeof(desiredFOV));
        file.seekp(0x0002431D); // VFOV
        file.write(reinterpret_cast<const char *>(&desiredFOV), sizeof(desiredFOV));

        // Confirmation message
        cout << "\nSuccessfully changed the field of view." << endl;

        // Close the file
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
