#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <limits>
#include <vector>
#include <cstdint> // For uint32_t variable type
#include <conio.h> // For getch() function [get character]
#include <string>
#include <algorithm>

using namespace std;

// Variables
string fileName;
uint32_t firstValue, secondValue, byteRange, number, currentValue;
size_t i, searchIndex;
int choice, tempChoice, j;
bool validKeyPressed;
char ch;

// Function to handle user input in choices
void HandleChoiceInput(int &choice)
{
    tempChoice = -1;         // Temporary variable to store the input
    validKeyPressed = false; // Flag to track if a valid key was pressed

    while (true)
    {
        ch = _getch(); // Waits for user to press a key

        // Checks if the key is '1' or '2'
        if ((ch == '1' || ch == '2') && !validKeyPressed)
        {
            tempChoice = ch - '0';  // Converts char to int and stores the input temporarily
            cout << ch;             // Echoes the valid input
            validKeyPressed = true; // Sets the flag as a valid key has been pressed
        }
        else if (ch == '\b' || ch == 127) // Handles backspace or delete keys
        {
            if (tempChoice != -1) // Checks if there is something to delete
            {
                tempChoice = -1;         // Resets the temporary choice
                cout << "\b \b";         // Erases the last character from the console
                validKeyPressed = false; // Resets the flag as the input has been deleted
            }
        }
        // If 'Enter' is pressed and a valid key has been pressed prior
        else if (ch == '\r' && validKeyPressed)
        {
            choice = tempChoice; // Assigns the temporary input to the choice variable
            cout << endl;        // Moves to a new line
            break;               // Exits the loop since we have a confirmed input
        }
    }
}

// Function to handle user input in resolution and byte search range
uint32_t HandleNumberInput()
{
    do
    {
        cin >> number;

        cin.clear();                                         // Clears error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (number <= 0 || number >= 65535)
        {
            cout << "Please enter a valid number." << endl;
        }
    } while (number <= 0 || number > 65535);

    return number;
}

int main()
{
    cout << "\nResolution Bytes Finder by AlphaYellow, 2024\n\n----------------" << endl;
    cout << "\n- Enter the file name: ";
    getline(cin, fileName);

    do
    {
        ifstream file(fileName, ios::binary);
        if (!file)
        {
            cerr << "\nCould not open the file." << endl;
            return 1;
        }

        cout << "\n- Enter the first 4-byte integer value: ";
        firstValue = HandleNumberInput();
        cout << "\n- Enter the second 4-byte integer value: ";
        secondValue = HandleNumberInput();
        cout << "\n- Enter the search range in bytes: ";
        byteRange = HandleNumberInput();
        cout << "\n";

        vector<pair<size_t, size_t>> foundPairs; // Vector to store pairs of addresses for the first and second values

        vector<char> buffer(istreambuf_iterator<char>(file), {});
        for (i = 0; i <= buffer.size() - 4; ++i)
        {
            memcpy(&currentValue, &buffer[i], sizeof(uint32_t));
            if (currentValue == firstValue)
            {
                for (j = -byteRange; j <= byteRange; ++j)
                {
                    searchIndex = i + j;
                    if (searchIndex >= 0 && searchIndex <= buffer.size() - 4)
                    {
                        uint32_t tempValue;
                        memcpy(&tempValue, &buffer[searchIndex], sizeof(uint32_t));
                        if (tempValue == secondValue)
                        {
                            foundPairs.push_back(make_pair(i, searchIndex));
                        }
                    }
                }
            }
        }

        if (!foundPairs.empty())
        {
            cout << "Found pairs within " << dec << byteRange << " bytes at the following addresses:" << endl;
            for (const auto &pair : foundPairs)
            {
                cout << "First value (" << dec << firstValue << ") at: " << uppercase << setfill('0') << setw(8) << hex << pair.first << ", ";
                cout << "Second value (" << dec << secondValue << ") at: " << uppercase << setfill('0') << setw(8) << hex << pair.second << endl;
            }
        }
        else
        {
            cout << "No pairs of first value (" << dec << firstValue << ") and second value (" << dec << secondValue << ") were found within " << dec << byteRange << " bytes." << endl;
        }
        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice);

        if (choice == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }
    } while (choice != 1); // Checks the flag in the loop condition
}