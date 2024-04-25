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
    cout << "The Mummy (2000) Widescreen Fix by AlphaYellow, 2024\n\n----------------\n\n";
    float aspectratio, fov, width, height;

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

    fstream file("MummyPC.exe", ios::in | ios::out | ios::binary);

    aspectratio = width / height;

    file.seekp(0x00082F5C);
    file.write(reinterpret_cast<const char *>(&aspectratio), sizeof(aspectratio));

    do
    {
        cout << "\nDo you want to fix the FOV automatically based on the resolution typed above (1) or set a custom FOV multiplier value (2) ?: ";
        cin >> choice;

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            choice = -1;                                         // Ensures the loop continues
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (choice < 1 || choice > 2)
        {
            cout << "Please enter a valid number." << endl;
        }
    } while (choice < 1 || choice > 2);

    switch (choice)
    {
    case 1:
        fov = aspectratio / (4.0f / 3.0f);
        file.seekp(0x000673E5);
        file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
        break;

    case 2:
        cout << "\nType a custom FOV multiplier value (default for 4:3 aspect ratio is 1,0): " << endl;
        cin >> fov;
        file.seekp(0x000673E5);
        file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
        break;
    }

    // Confirmation message
    cout << "\nSuccessfully changed the aspect ratio and field of view. You can now press Enter to close the program." << endl;

    // Close the file
    file.close();
    getch();

    return 0;
}