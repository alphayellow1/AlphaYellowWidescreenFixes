#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h>
#include <cstdint>
#include <cmath>
#include <limits>

// Function to calculate the greatest common divisor
int gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
}

using namespace std;

int main()
{
    double newWidth, newHeight, fov2;
    float newAspectRatio, fov1;
    int simplifiedWidth, simplifiedHeight, aspectRatioGCD;

    cout << "The Lord of the Rings: The Fellowship of the Ring (2002) Widescreen Fix by AlphaYellow, 2024\n\n----------------\n\n";

    do
    {
        cout << "\n- Enter the desired width: ";
        cin >> newWidth;

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            newWidth = -1;                                       // Ensures the loop continues
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newWidth <= 0 || newWidth > 65535)
        {
            cout << "Please enter a positive number for width less than 65536." << endl;
        }
    } while (newWidth <= 0 || newWidth > 65535);

    do
    {
        cout << "\n- Enter the desired height: ";
        cin >> newHeight;

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            newHeight = -1;                                      // Ensures the loop continues
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (newHeight <= 0 || newHeight > 65535)
        {
            cout << "Please enter a positive number for height less than 65536." << endl;
        }
    } while (newHeight <= 0 || newHeight > 65535);

    // Calculates the greatest common divisor of the width and height
    aspectRatioGCD = gcd(static_cast<int>(newWidth), static_cast<int>(newHeight));

    // Simplifies the width and height by dividing by the greatest common divisor
    simplifiedWidth = static_cast<int>(newWidth) / aspectRatioGCD;
    simplifiedHeight = static_cast<int>(newHeight) / aspectRatioGCD;

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

    newAspectRatio = newWidth / newHeight;
    
    fstream file("Fellowship.exe", ios::in | ios::out | ios::binary);
    if (!file.is_open())
    {
        cout << "Failed to open the file." << endl;
        return 1;
    }

    // Puts the aspect ratio value in the AR address
    file.seekp(0x0011D948);
    file.write(reinterpret_cast<const char *>(&newAspectRatio), sizeof(newAspectRatio));

    // Puts the FOV value in the FOV address
    fov1 = 64 / (newAspectRatio * 0.75);
    file.seekp(0x001208CC);
    file.write(reinterpret_cast<const char *>(&fov1), sizeof(fov1));
    fov2 = fov1;
    file.seekp(0x00120A90);
    file.write(reinterpret_cast<const char *>(&fov2), sizeof(fov2));

    // Confirmation message
    cout << "\nSuccessfully changed the aspect ratio to " << simplifiedWidth << ":" << simplifiedHeight << " and fixed the field of view in the executable. Now all " << simplifiedWidth << ":" << simplifiedHeight << " resolutions supported by the graphics card's driver will appear in the game's video settings." << endl;

    // Closes the file
    file.close();

    cout << "Press enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}