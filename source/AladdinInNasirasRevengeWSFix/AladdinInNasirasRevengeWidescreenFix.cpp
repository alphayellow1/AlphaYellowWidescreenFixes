#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    int arWrittenToFile, fovWrittenToFile, choice, fileOpened;
    cout << "Aladdin in Nasira's Revenge (2001) Widescreen Fixer v1.3 by AlphaYellow, 2024\n\n----------------\n";
    float width, height, aspectRatio, FOV;
    bool fileNotFound;

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        // Tries to open the file initially
        file.open("aladdin.exe", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open aladdin.exe, check if the executable has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the executable is currently running. Press Enter when all the mentioned problems are solved." << endl;
            cin.get();

            // Tries to open the file again
            file.open("aladdin.exe", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\naladdin.exe opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

        do
        {
            arWrittenToFile = 0;
            fovWrittenToFile = 0;
            cout << "\nEnter the desired width: ";
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

        aspectRatio = (4.0f / 3.0f) / (width / height);

        // Adjust the result for the new base FOV
        aspectRatio *= 0.75f; // Scale the result by the ratio of the new base FOV to the old base FOV

        file.seekp(0x000115E9);
        file.write(reinterpret_cast<const char *>(&aspectRatio), sizeof(aspectRatio));
        arWrittenToFile = 1;

        cout << "\nDo you want to fix the FOV automatically based on the resolution typed above (1) or set a custom FOV multiplier value (2)?: ";
        cin >> choice;

        switch (choice)
        {
        case 1:
            if (aspectRatio == 0.75)
            {
                FOV = 0.8546222448; // 4:3
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.5625)
            {
                FOV = 1.139; // 16:9
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.625)
            {
                FOV = 1.025; // 16:10
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.8)
            {
                FOV = 0.8; // 5:4
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.6)
            {
                FOV = 1.0674; // 15:9
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.421875)
            {
                FOV = 1.518; // 21:9 (2560x1080)
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.41860465116)
            {
                FOV = 1.53; // 21:9 (3440x1440)
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.4166666)
            {
                FOV = 1.54; // 21:9 (3840x1600)
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.3125)
            {
                FOV = 2.05; // 32:10
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.28125)
            {
                FOV = 2.28; // 32:9
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.2666666)
            {
                FOV = 2.4; // 15:4
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.25)
            {
                FOV = 2.565; // 12:3
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.2083333)
            {
                FOV = 3.076; // 48:10
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.2)
            {
                FOV = 3.205; // 45:9
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
                fovWrittenToFile = 1;
            }
            else if (aspectRatio == 0.1875)
            {
                FOV = 3.42; // 48:9
                file.seekp(0x0009C206);
                file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
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
            cin >> FOV;
            file.seekp(0x0009C206);
            file.write(reinterpret_cast<const char *>(&FOV), sizeof(FOV));
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

        do
        {
            cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
            cin >> choice;

            if (cin.fail())
            {
                cin.clear();                                         // Clears error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                choice = -1;
                cout << "Invalid input. Please enter 1 to exit the program or 2 to try another value." << endl;
            }
            else if (choice < 1 || choice > 2)
            {
                cout << "Please enter a valid number." << endl;
            }
        } while (choice < 1 || choice > 2);
    } while (choice == 2); // Checks the flag in the loop condition

    cout << "\nPress enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}