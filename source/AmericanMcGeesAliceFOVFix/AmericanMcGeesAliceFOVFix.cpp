#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    cout << "American McGee's Alice (2000) FOV Fixer by AlphaYellow, 2024\n\n----------------\n\n";
    float fov;

    do
    {
        cout << "\n- Enter the desired FOV (1-180º): ";
        cin >> fov;

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            fov = -1;                                  // Ensures the loop continues
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (fov <= 1 || fov >= 180)
        {
            cout << "Please enter a valid number for the FOV." << endl;
        }
    } while (fov <= 1 || fov >= 180);

    fstream file("fgamex86.dll", ios::in | ios::out | ios::binary);

    file.seekp(0x000A558E);
    file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));
    file.seekp(0x0006CACF);
    file.write(reinterpret_cast<const char *>(&fov), sizeof(fov));

    // Confirmation message
    cout << "\nSuccessfully changed field of view in the executable.\n"
         << endl;

    // Closes the file
    file.close();

    cout << "Press enter to exit the program...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clears the input buffer
    cin.get();

    return 0;
}
