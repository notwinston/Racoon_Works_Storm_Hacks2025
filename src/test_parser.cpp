#include "parser.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    Parser parser;
    
    std::cout << "Parsing file: " << filename << std::endl;
    
    if (parser.parseFile(filename)) {
        std::cout << "Parsing successful!" << std::endl;
        parser.printParsedData();
    } else {
        std::cerr << "Parsing failed!" << std::endl;
        return 1;
    }
    
    return 0;
}
