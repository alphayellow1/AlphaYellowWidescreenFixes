#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    cout << "The Chickenator (2003) FOV Fixer by AlphaYellow, 2024\n\n----------------\n\n";
    float width, height, fov;

    do
    {
        cout << "Enter the desired width: ";
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
        cout << "\nEnter the desired height: ";
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

    fstream file("chickenator.exe", ios::in | ios::out | ios::binary);

    fov = (4.0f / 3.0f) / (width / height);
    file.seekp(0x00127080);
    file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

    // Confirmation message
    cout << "\nSuccessfully changed field of view in the executable.\n" << endl;

    // Closes the file
    file.close();

    cout << "Press enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}
