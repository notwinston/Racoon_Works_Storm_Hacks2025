#include "parser.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

Parser::Parser() : memory_limit_(0) {}

bool Parser::parseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }
    
    std::string line;
    int lineNumber = 0;
    bool firstLine = true;
    
    while (std::getline(file, line)) {
        lineNumber++;
        
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        
        // Parse first line for memory limit
        if (firstLine) {
            std::istringstream iss(line);
            std::string returnKeyword;
            if (!(iss >> returnKeyword >> memory_limit_)) {
                std::cerr << "Error: Invalid first line format at line " << lineNumber << std::endl;
                return false;
            }
            if (returnKeyword != "Return") {
                std::cerr << "Error: Expected 'Return' keyword at line " << lineNumber << std::endl;
                return false;
            }
            firstLine = false;
            continue;
        }
        
        // Parse node lines
        if (!parseLine(line, lineNumber)) {
            return false;
        }
    }
    
    file.close();
    return true;
}

bool Parser::parseLine(const std::string& line, int lineNumber) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    
    // Tokenize the line
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    if (tokens.size() < 6) {
        std::cerr << "Error: Invalid line format at line " << lineNumber 
                  << " (expected at least 6 tokens, got " << tokens.size() << ")" << std::endl;
        return false;
    }
    
    try {
        int nodeId = std::stoi(tokens[0]);
        std::string operationName = tokens[1];
        int numInputs = std::stoi(tokens[2]);
        
        // Validate number of inputs
        if (numInputs < 0 || numInputs > 10) { // Reasonable upper limit
            std::cerr << "Error: Invalid number of inputs at line " << lineNumber << std::endl;
            return false;
        }
        
        // Check if we have enough tokens for the inputs
        if (tokens.size() < 6 + numInputs) {
            std::cerr << "Error: Not enough tokens for inputs at line " << lineNumber << std::endl;
            return false;
        }
        
        // Parse input node IDs
        std::vector<int> inputIds;
        for (int i = 0; i < numInputs; i++) {
            inputIds.push_back(std::stoi(tokens[3 + i]));
        }
        
        // Parse memory and time values
        int runMem = std::stoi(tokens[3 + numInputs]);
        int outputMem = std::stoi(tokens[4 + numInputs]);
        int timeCost = std::stoi(tokens[5 + numInputs]);
        
        // Create the node
        Node node = createNode(operationName, inputIds, runMem, outputMem, timeCost);
        
        // Store the node
        node_map_[nodeId] = node;
        nodes_.push_back(node);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to parse line " << lineNumber << ": " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

Node Parser::createNode(const std::string& name, 
                       const std::vector<int>& inputIds,
                       int runMem, 
                       int outputMem, 
                       int timeCost) {
    // Create input nodes vector
    std::vector<Node> inputNodes;
    for (int inputId : inputIds) {
        // For now, create placeholder nodes for inputs
        // In a more sophisticated implementation, you might want to reference existing nodes
        Node inputNode;
        inputNodes.push_back(inputNode);
    }
    
    return Node(name, inputNodes, runMem, outputMem, timeCost);
}

void Parser::printParsedData() const {
    std::cout << "Memory Limit: " << memory_limit_ << std::endl;
    std::cout << "Number of nodes: " << nodes_.size() << std::endl;
    std::cout << "\nNodes:" << std::endl;
    
    for (size_t i = 0; i < nodes_.size(); i++) {
        const Node& node = nodes_[i];
        std::cout << "Node " << i << ": " << node.getName() 
                  << " (run_mem: " << node.getRunMem() 
                  << ", output_mem: " << node.getOutputMem() 
                  << ", time_cost: " << node.getTimeCost() << ")" << std::endl;
    }
}
