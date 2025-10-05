#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "Node.hpp"

class Parser {
private:
    int memory_limit_;
    std::vector<Node> nodes_;
    std::map<int, Node> node_map_; // Maps node ID to Node object

public:
    Parser();
    
    // Parse input file and populate nodes
    bool parseFile(const std::string& filename);
    
    // Getters
    int getMemoryLimit() const { return memory_limit_; }
    const std::vector<Node>& getNodes() const { return nodes_; }
    const std::map<int, Node>& getNodeMap() const { return node_map_; }
    
    // Helper methods
    void printParsedData() const;
    
private:
    // Helper method to parse a single line
    bool parseLine(const std::string& line, int lineNumber);
    
    // Helper method to create Node from parsed data
    Node createNode(const std::string& name, 
                   const std::vector<int>& inputIds,
                   int runMem, 
                   int outputMem, 
                   int timeCost);
};

#endif // PARSER_HPP
