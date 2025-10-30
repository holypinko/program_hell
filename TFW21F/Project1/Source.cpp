#include <iostream>
#include <fstream>
#include <random>
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
using namespace std;
void shredder(const wstring& filename) {
    ifstream input(filename, ios::binary);
    if (!input) {
        cerr << "error opening file" << endl;
        return;
    }
    input.seekg(0, ios::end);
    size_t filesize = input.tellg();
    input.seekg(0, ios::beg);
    char* buffer = new char[filesize];
    input.read(buffer, filesize);
    input.close();
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < filesize / 10; ++i) {
        buffer[i] = static_cast<char>(dis(gen));
    }

    ofstream output(filename, ios::binary);
    output.write(buffer, filesize);
    delete[] buffer;
}
void crusher(const wstring& filename) {
    ifstream input(filename, ios::binary);
    if (!input) {
        cerr << "error opening file" << endl;
        return;
    }
    input.seekg(0, ios::end);
    size_t filesize = input.tellg();
    input.seekg(0, ios::beg);
    char* buffer = new char[filesize];
    input.read(buffer, filesize);
    input.close();
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);
    for (size_t i = 0; i < filesize; i += 2) {
        buffer[i] = static_cast<char>(dis(gen));
    }
    ofstream output(filename, ios::binary); output.write(buffer, filesize); delete[]buffer;
}
void lobotomy(const wstring& filename) { ifstream input(filename, ios::binary); if (!input) { cerr << "Error opening file." << endl; return; }input.seekg(0, ios::end); size_t filesize = input.tellg(); input.seekg(0, ios::beg); char* buffer = new char[filesize]; input.read(buffer, filesize); input.close(); random_device rd; mt19937 gen(rd()); uniform_int_distribution<> dis(0, 255); for (size_t i = 0; i < filesize; ++i) { buffer[i] = static_cast<char>(dis(gen)); }ofstream output(filename, ios::binary); output.write(buffer, filesize); delete[]buffer; }
int main() {
    OPENFILENAME ofn; WCHAR szFile[260] = { 0 }; ZeroMemory(&ofn, sizeof(ofn)); ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = NULL; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFilter = L"All Files(.)\0*.*\0"; ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST; if (GetOpenFileName(&ofn) == TRUE) {
        wstring filename(szFile);
        cout << "Select corruption method : \n";
        cout << "1.Little Boy (corrupts first 10 % of file data, still semi-recoverable)\n";
        cout << "2.Fallout (corrupt every other byte)\n";
        cout << "3.Fat Man (destroys 99.9-100% of recoverable data)\n";
        int choice;
        cin >> choice;
        switch (choice) {
        case 1:
            shredder(filename);
            break;
        case 2:
            crusher(filename);
            break;
        case 3:
            lobotomy(filename);
            break;
        default:
            cout << "invalid choice. pick another one bitch\n";
            return 1;
        }
        cout << "file destroyed\n";

        ShellExecute(NULL, L"open", L"https://www.youtube.com/watch?v=6xdKTeqQcpo", NULL, NULL, SW_SHOWNORMAL);}
        return 0;
    }