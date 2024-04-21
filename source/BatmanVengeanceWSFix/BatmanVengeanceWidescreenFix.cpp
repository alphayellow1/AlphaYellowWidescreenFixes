#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h>
#include <cstdint>
#include <limits>

using namespace std;

int main()
{
    cout << "Batman: Vengeance (2002) Widescreen Fix by AlphaYellow, 2024\n\n----------------\n\n";
    int width, height;
    float fov;

    do
    {
        cout << "Enter the desired width: ";
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
        cout << "\nEnter the desired height: ";
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

    fstream file("osr_dx8_vf.dll", ios::in | ios::out | ios::binary);

    file.seekp(0x00098AE8);
    file.write(reinterpret_cast<const char *>(&width), sizeof(width));

    file.seekp(0x00098AEC);
    file.write(reinterpret_cast<const char *>(&height), sizeof(height));

    fov = (4.0f / 3.0f) / (width / height);
    file.seekp(0x000862A0);
    file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

    // Confirmation message
    cout << "\nSuccessfully changed resolution to " << width << "x" << height << " and fixed field of view in the executable." << endl;

    // Close the file
    file.close();

    cout << "Press Enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}
