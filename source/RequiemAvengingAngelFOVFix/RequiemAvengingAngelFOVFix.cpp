#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

int main()
{
    int language;
    cout << "Requiem: Avenging Angel Widescreen Fix by AlphaYellow, 2024\n\n----------------\n\n";
    float width, height, result;
    union
    {
        float input;
        unsigned int output;
    } data;

    do
    {
        cout << "Enter the desired width: ";
        cin >> width;

        if (cin.fail())
        {
            cin.clear();                                                   // Clear error flags
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
            width = -1;                                                         // Ensure the loop continues
            cout << "Invalid input. Please enter a numeric value." << std::endl;
        }
        else if (width <= 0 || width > 65535)
        {
            cout << "Please enter a positive number for width less than 65536." << std::endl;
        }
    } while (width <= 0 || width > 65535);

    do
    {
        cout << "Enter the desired height: ";
        cin >> height;

        if (cin.fail())
        {
            cin.clear();                                                   // Clear error flags
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
            height = -1;                                                        // Ensure the loop continues
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (height <= 0 || height > 65535)
        {
            std::cout << "Please enter a positive number for height less than 65536." << endl;
        }
    } while (height <= 0 || height > 65535);

    fstream file("D3D.exe", ios::in | ios::out | ios::binary);
    fstream file("D3D.exe", ios::in | ios::out | ios::binary);

    result = (4.0f / 3.0f) / (width / height);
    const unsigned char *pf = reinterpret_cast<const unsigned char *>(&result);
    for (size_t i = 0; i != sizeof(float); ++i)
    {
        file.seekp(0x000242E6 + i);
        file << char(pf[i]); 
        // printf("%X ", pf[i]);
    }

    // Confirmation message
    cout << "Successfully changed field of view in the executable. You can close the program now." << endl;

    // Close the file
    file.close();
    getch();

    return 0;
}
