#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    cout << "Croc 2 (2000) Widescreen Fix by AlphaYellow, 2024\n\n----------------\n";
    float desiredWidth, desiredHeight, fov;
    int choice, fileOpened;
    bool fileNotFound;

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initialize fileOpened to 0

        // Try to open the file initially
        file.open("Croc2.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, set fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loop until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open Croc2.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            getch(); // Wait for user input

            // Try to open the file again
            file.open("Croc2.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nCroc2.exe opened successfully!" << endl;
                    fileOpened = 1; // Set fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Set fileNotFound to false as the file is found and opened
            }
        }

        do
        {
            cout << "\n- Enter the desired width: ";
            cin >> desiredWidth;

            if (cin.fail())
            {
                cin.clear();                                                   // Clear error flags
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
                desiredWidth = -1;                                             // Ensure the loop continues
                cout << "Invalid input. Please enter a numeric value." << std::endl;
            }
            else if (desiredWidth <= 0 || desiredWidth > 65535)
            {
                cout << "Please enter a positive number for width less than 65536." << std::endl;
            }
        } while (desiredWidth <= 0 || desiredWidth > 65535);

        do
        {
            cout << "\n- Enter the desired height: ";
            cin >> desiredHeight;

            if (cin.fail())
            {
                cin.clear();                                                   // Clear error flags
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
                desiredHeight = -1;                                            // Ensure the loop continues
                cout << "Invalid input. Please enter a numeric value." << endl;
            }
            else if (desiredHeight <= 0 || desiredHeight > 65535)
            {
                std::cout << "Please enter a positive number for height less than 65536." << endl;
            }
        } while (desiredHeight <= 0 || desiredHeight > 65535);

        fov = ((4.0f / 3.0f) / (desiredWidth / desiredHeight)) * 0.75f;

        file.seekp(0x000141E9);
        file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

        cout << "\nSuccessfully changed the aspect ratio and field of view." << endl;

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