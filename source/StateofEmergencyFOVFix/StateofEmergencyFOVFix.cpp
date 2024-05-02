#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    int choice;
    cout << "State of Emergency (2003) FOV Fixer by AlphaYellow, 2024\n\n----------------\n";
    float hFOV, desiredWidth, desiredHeight;

    do
    {
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

        fstream file("KaosPC.exe", ios::in | ios::out | ios::binary);

        hFOV = (desiredWidth / desiredHeight) / (4.0f / 3.0f);

        file.seekp(0x000B77C0);
        file.write(reinterpret_cast<const char *>(&hFOV), sizeof(hFOV));

        // Confirmation message
        cout << "\nSuccessfully changed the field of view." << endl;

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