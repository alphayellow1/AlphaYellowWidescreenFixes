#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint> // For uint8_t
#include <limits>
#include <windows.h>

// Function to calculate the greatest common divisor
int gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
}

using namespace std;

int main()
{
    cout << "Adventure Pinball: Forgotten Island (2001) Resolution Fixer by AlphaYellow, 2024\n\n----------------\n";
    uint16_t desiredWidth, desiredHeight;
    int choice, choice2, fileOpened, simplifiedWidth, simplifiedHeight, aspectRatioGCD;
    bool fileNotFound, validInput = false;
    double aspectRatio, standardRatio, desiredWidth2, desiredHeight2;

    do
    {
        fstream file;
        fileNotFound = false;
        fileOpened = 0; // Initializes fileOpened to 0

        TCHAR buffer[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, buffer);

        // Tries to open the file initially
        file.open("D3DDrv.dll", ios::in | ios::out | ios::binary);

        // If the file is not open, sets fileNotFound to true
        if (!file.is_open())
        {
            fileNotFound = true;
        }

        // Loops until the file is found and opened
        while (fileNotFound)
        {
            cout << "\nFailed to open D3DDrv.dll, check if the file has special permissions allowed that prevent the fixer from opening it (e.g: read-only mode), it's not present in the same directory as the fixer, or if the file is currently being used. Press Enter when all the mentioned problems are solved." << endl;
            cin.clear();                                         // Clear error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
            cin.get();                                           // Waits for the user to press a key

            // Tries to open the file again
            file.open("D3DDrv.dll", ios::in | ios::out | ios::binary);

            if (file.is_open())
            {
                if (fileOpened == 0)
                {
                    cout << "\nD3DDrv.dll opened successfully!" << endl;
                    fileOpened = 1; // Sets fileOpened to 1 after the file is opened successfully
                }
                fileNotFound = false; // Sets fileNotFound to false as the file is found and opened
            }
        }

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
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (desiredWidth <= 0 || desiredWidth > 65535)
                {
                    cout << "Please enter a positive number for width less than 65536." << endl;
                }
                else
                {
                    validInput = true; // Sets flag to true if input is valid
                }
            } while (!validInput); // Continues loop until valid input is received

            validInput = false; // Resets flag for height input

            do
            {
                cout << "\n- Enter the desired height: ";
                cin >> desiredHeight;

                if (cin.fail())
                {
                    cin.clear();                                         // Clears error flags
                    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                    cout << "Invalid input. Please enter a numeric value." << endl;
                }
                else if (desiredHeight <= 0 || desiredHeight > 65535)
                {
                    cout << "Please enter a positive number for height less than 65536." << endl;
                }
                else
                {
                    validInput = true; // Sets flag to true if input is valid
                }
            } while (!validInput); // Continues loop until valid input is received

            // Calculates the greatest common divisor of the width and height
            aspectRatioGCD = gcd(static_cast<int>(desiredWidth), static_cast<int>(desiredHeight));

            // Simplifies the width and height by dividing by the greatest common divisor
            simplifiedWidth = static_cast<int>(desiredWidth) / aspectRatioGCD;
            simplifiedHeight = static_cast<int>(desiredHeight) / aspectRatioGCD;

            if (simplifiedWidth == 16 && simplifiedHeight == 3)
            {
                simplifiedWidth = 48;
                simplifiedHeight = 9;
            }
            else if (simplifiedWidth == 8 && simplifiedHeight == 5)
            {
                simplifiedWidth = 16;
                simplifiedHeight = 10;
            }
            else if (simplifiedWidth == 7 && simplifiedHeight == 3)
            {
                simplifiedWidth = 21;
                simplifiedHeight = 9;
            }
            else if (simplifiedWidth == 5 && simplifiedHeight == 1)
            {
                simplifiedWidth = 45;
                simplifiedHeight = 9;
            }

            desiredWidth2 = static_cast<double>(desiredWidth);
            desiredHeight2 = static_cast<double>(desiredHeight);
            aspectRatio = desiredWidth2 / desiredHeight2;
            standardRatio = 16.0 / 9.0; // 16:9 aspect ratio

            // Initializes choice2 to 1 to allow exiting the loop if aspect ratio is not wider
            choice2 = 1;

            // Checks if the calculated aspect ratio is wider than 16:9
            if (aspectRatio > standardRatio)
            {
                cout << "\nWarning: The aspect ratio of " << desiredWidth << "x" << desiredHeight << ", which is " << simplifiedWidth << ":" << simplifiedHeight << ", is wider than 16:9, menus will be too cropped and become unusable! Are you sure you want to proceed (1) or want to insert a less wider resolution closer to 16:9 (2)?: " << endl;
                do
                {
                    cin >> choice2;

                    if (cin.fail())
                    {
                        cin.clear();                                         // Clears error flags
                        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
                        choice2 = -1;
                        cout << " Invalid input. Please enter 1 to proceed or 2 to insert a new resolution." << endl;
                    }
                    else if (choice2 < 1 || choice2 > 2)
                    {
                        cout << " Please enter a valid number." << endl;
                    }
                } while (choice2 < 1 || choice2 > 2);
            }
        } while (choice2 != 1);

        file.seekp(0x000063C9); // 640
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x000063D4); // 480
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x000063DB); // 800
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x000063E4); // 600
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x000063EB); // 1024
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x000063F4); // 768
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x000063FB); // 1280
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x00006404); // 1024
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));
        file.seekp(0x0000640B); // 1600
        file.write(reinterpret_cast<const char *>(&desiredWidth), sizeof(desiredWidth));
        file.seekp(0x00006414); // 1200
        file.write(reinterpret_cast<const char *>(&desiredHeight), sizeof(desiredHeight));

        cout << "\nSuccessfully changed the resolution. Now open PB.ini inside " << buffer << "\\ and set FullscreenViewportX=" << desiredWidth << " and FullscreenViewportY=" << desiredHeight << " under the [WinDrv.WindowsClient] section." << endl;

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