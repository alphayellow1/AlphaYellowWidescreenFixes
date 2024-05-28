#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch() function
#include <limits>

using namespace std;

int main()
{
    cout << "Harry Potter: Quidditch World Cup (2003) FOV Fix by AlphaYellow, 2024\n\n----------------\n";
    float currentAspectRatio, newAspectRatio, desiredWidth, desiredHeight;
    int choice;

    do
    {
        

        do
        {
            cout << "\n- Enter the desired width: ";
            cin >> desiredWidth;

            if (cin.fail())
            {
                cin.clear();                                                   // Clears error flags
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignores invalid input
                desiredWidth = -1;                                             // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value.\n" << std::endl;
            }
            else if (desiredWidth <= 0 || desiredWidth > 65535)
            {
                cout << "Please enter a positive number for width less than 65536.\n" << std::endl;
            }
        } while (desiredWidth <= 0 || desiredWidth > 65535);

        do
        {
            cout << "\n- Enter the desired height: ";
            cin >> desiredHeight;

            if (cin.fail())
            {
                cin.clear();                                                   // Clears error flags
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignores invalid input
                desiredHeight = -1;                                            // Ensures the loop continues
                cout << "Invalid input. Please enter a numeric value.\n" << endl;
            }
            else if (desiredHeight <= 0 || desiredHeight > 65535)
            {
                std::cout << "Please enter a positive number for height less than 65536.\n" << endl;
            }
        } while (desiredHeight <= 0 || desiredHeight > 65535);

        newAspectRatio = desiredWidth / desiredHeight;

        file.seekp(0x0009F445);
        file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

        // Confirmation message
        cout << "\nSuccessfully changed the field of view.\n"
             << endl;

        // Closes the file
        file.close();

        do
        {
            cout << "- Do you want to exit the program (1) or try another value (2)?: ";
            cin >> choice;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice = -1;
                cout << "Invalid input. Please enter 1 to exit or 2 to try another value.\n" << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << "Please enter a valid number.\n" << endl;
            }
        } while (choice < 1 || choice > 2);

    } while (choice != 1); // Checks the flag in the loop condition

    cout << "\nPress Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}