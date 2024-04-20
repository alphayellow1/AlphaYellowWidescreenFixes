#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h> // For getch()
#include <cstdint> // For uint8_t
#include <limits>

using namespace std;

// Function to swap the byte order of a 32-bit value
unsigned int swapEndian(unsigned int value)
{
    return (value >> 24) |
           ((value << 8) & 0x00FF0000) |
           ((value >> 8) & 0x0000FF00) |
           (value << 24);
}

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
        std::cout << "Enter the desired width: ";
        std::cin >> width;

        if (std::cin.fail())
        {
            std::cin.clear();                                                   // Clear error flags
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
            width = -1;                                                         // Ensure the loop continues
            std::cout << "Invalid input. Please enter a numeric value." << std::endl;
        }
        else if (width <= 0 || width > 65535)
        {
            std::cout << "Please enter a positive number for width less than 65536." << std::endl;
        }
    } while (width <= 0 || width > 65535);

    do
    {
        std::cout << "Enter the desired height: ";
        std::cin >> height;

        if (std::cin.fail())
        {
            std::cin.clear();                                                   // Clear error flags
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore invalid input
            height = -1;                                                        // Ensure the loop continues
            std::cout << "Invalid input. Please enter a numeric value." << std::endl;
        }
        else if (height <= 0 || height > 65535)
        {
            std::cout << "Please enter a positive number for height less than 65536." << std::endl;
        }
    } while (height <= 0 || height > 65535);

    // Apply the formula
    result = (4.0f / 3.0f) / (width / height);
    data.input = result;

    // Swap the byte order to big endian
    unsigned int bigEndianValue = swapEndian(data.input);

    // Predefined byte address
    const unsigned int address = 0x000242E6;

    // File operations
    fstream file("D3D.exe", ios::in | ios::out | ios::binary);
    if (!file)
    {
        cout << "File could not be opened." << endl;
        getch();
        return 1; // Exit if the file cannot be opened
    }

    cout << "Big endian value: " << hex << bigEndianValue << "\n";
    cout << "Data output: " << hex << data.output << "\n";

    // Seek to the position and write the big endian value
    file.seekp(address, ios::beg);
    file.write(reinterpret_cast<char *>(&bigEndianValue), sizeof(bigEndianValue));

    // Confirmation message
    cout << "Successfully changed field of view in the executable. You can close the program now." << endl;

    // Close the file
    file.close();
    getch();

    return 0;
}
