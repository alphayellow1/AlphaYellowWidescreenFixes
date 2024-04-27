#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    int arWrittenToFile, fovWrittenToFile, choice;
    cout << "Aladdin in Nasira's Revenge (2001) FOV Fixer by AlphaYellow, 2024\n\n----------------\n\n";
    float width, height, aspectratio, fov;

    do
    {
        arWrittenToFile = 0;
        fovWrittenToFile = 0;
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

    fstream file("aladdin.exe", ios::in | ios::out | ios::binary);

    aspectratio = (4.0f / 3.0f) / (width / height);

    // Adjust the result for the new base FOV
    aspectratio *= 0.75f; // Scale the result by the ratio of the new base FOV to the old base FOV

    file.seekp(0x000115E9);
    file.write(reinterpret_cast<const char *>(&aspectratio), sizeof(aspectratio));
    arWrittenToFile = 1;

    cout << "\nDo you want to fix the FOV automatically based on the resolution typed above (1) or set a custom FOV multiplier value (2) ?: ";
    cin >> choice;

    switch (choice)
    {
    case 1:
        if (aspectratio == 0.75)
        {
            fov = 0.8546222448; // 4:3
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.5625)
        {
            fov = 1.139; // 16:9
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.625)
        {
            fov = 1.025; // 16:10
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.8)
        {
            fov = 0.8; // 5:4
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.6)
        {
            fov = 1.0674; // 15:9
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.421875)
        {
            fov = 1.518; // 21:9 (2560x1080)
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.41860465116)
        {
            fov = 1.53; // 21:9 (3440x1440)
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.4166666)
        {
            fov = 1.54; // 21:9 (3840x1600)
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.3125)
        {
            fov = 2.05; // 32:10
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.28125)
        {
            fov = 2.28; // 32:9
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.2666666)
        {
            fov = 2.4; // 15:4
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.25)
        {
            fov = 2.565; // 12:3
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.2083333)
        {
            fov = 3.076; // 48:10
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.2)
        {
            fov = 3.205; // 45:9
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else if (aspectratio == 0.1875)
        {
            fov = 3.42; // 48:9
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
            fovWrittenToFile = 1;
        }
        else
        {
            cout << "\nThe FOV on this aspect ratio can't yet be fixed with this patch, please contact AlphaYellow on the mod's page or through Discord (alphayellow) to add support for it or set a custom FOV multiplier value." << endl;
            fovWrittenToFile = 0;
        }
        break;

    case 2:
        cout << "\nType a custom FOV multiplier value (default for 4:3 aspect ratio is 0,8546222448): " << endl;
        cin >> fov;
        file.seekp(0x0009C206);
        file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
        fovWrittenToFile = 1;
        break;
    }

    // Confirmation message
    if (arWrittenToFile == 1 && fovWrittenToFile == 0)
    {
        cout << "\nSuccessfully changed the aspect ratio in the executable. You can now press Enter to close the program." << endl;
    }
    else if (arWrittenToFile == 1 && fovWrittenToFile == 1)
    {
        cout << "\nSuccessfully changed the aspect ratio and field of view in the executable. You can now press Enter to close the program." << endl;
    }

    // Close the file
    file.close();
    getch();

    return 0;
}