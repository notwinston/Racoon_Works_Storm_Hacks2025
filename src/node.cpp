// ============================================================================
// NODE CLASS IMPLEMENTATION - Enhanced for Research Paper Algorithm
// ============================================================================

#include "node.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>

// ============================================================================
// NODE IMPLEMENTATION
// ============================================================================

Node::Node(std::string name, std::vector<std::string> inputs, 
           int run_mem, int output_mem, int time_cost)
    : name_(name), input_names_(inputs), run_mem_(run_mem), 
      output_mem_(output_mem), time_cost_(time_cost) {
    
    // PBC model from paper: node produces output before consuming inputs
    // Peak = max memory needed during execution
    peak_ = std::max(run_mem_, output_mem_);
    
    // Impact = net memory change (initially just output, updated later based on freed inputs)
    impact_ = output_mem_;
}

bool Node::hasInput(const std::string& input_name) const {
    return std::find(input_names_.begin(), input_names_.end(), input_name) != input_names_.end();
}

// ============================================================================
// SCHEDULE STATE IMPLEMENTATION
// ============================================================================

void ScheduleState::addNode(const std::string& node_name, int time_cost, int output_mem) {
    execution_order.push_back(node_name);
    computed.insert(node_name);
    total_time += time_cost;
    current_memory += output_mem;
    output_memory[node_name] = output_mem;
    memory_peak = std::max(memory_peak, current_memory);
}

void ScheduleState::freeMemory(int memory_amount) {
    current_memory = std::max(0, current_memory - memory_amount);
}

bool ScheduleState::isComputed(const std::string& node_name) const {
    return computed.find(node_name) != computed.end();
}

// ============================================================================
// INPUT PARSER IMPLEMENTATION
// ============================================================================

InputParser::InputParser(const std::string& filename) : filename_(filename), total_memory_(0) {}

bool InputParser::parse() {
    std::ifstream file(filename_);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename_ << std::endl;
        return false;
    }
    
    std::string line;
    bool first_line = true;
    
    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string token;
        
        if (first_line) {
            // First line: "Return <total_memory>"
            iss >> token; // "Return"
            iss >> total_memory_;
            first_line = false;
        } else {
            // Parse node line: <node_id> <operation> <input_count> <input1> <input2> ... <run_mem> <output_mem> <time_cost>
            int node_id;
            std::string operation;
            int input_count;
            
            iss >> node_id >> operation >> input_count;
            
            // Parse input dependencies
            std::vector<std::string> inputs;
            for (int i = 0; i < input_count; ++i) {
                int input_id;
                iss >> input_id;
                inputs.push_back(std::to_string(input_id));
            }
            
            // Parse memory and time values
            int run_mem, output_mem, time_cost;
            iss >> run_mem >> output_mem >> time_cost;
            
            // Create node with string ID
            std::string node_name = std::to_string(node_id);
            Node node(node_name, inputs, run_mem, output_mem, time_cost);
            nodes_[node_name] = node;
        }
    }
    
    file.close();
    
    // Build dependency graph
    buildDependencyGraph();
    
    // Calculate accurate impact values
    calculateImpactValues();
    
    return true;
}

void InputParser::buildDependencyGraph() {
    // Build successors and dependencies maps
    for (const auto& [node_name, node] : nodes_) {
        for (const auto& input_name : node.getInputs()) {
            // Add to successors map
            successors_[input_name].push_back(node_name);
            
            // Add to dependencies map
            dependencies_[input_name].insert(node_name);
        }
    }
}

void InputParser::calculateImpactValues() {
    // Calculate accurate impact values based on when inputs can be freed
    for (auto& [node_name, node] : nodes_) {
        int freed_memory = 0;
        
        // Check which inputs can be freed after this node executes
        for (const auto& input_name : node.getInputs()) {
            auto dep_it = dependencies_.find(input_name);
            if (dep_it != dependencies_.end()) {
                // If this node is the only consumer, we can free the input
                if (dep_it->second.size() == 1 && dep_it->second.count(node_name) > 0) {
                    auto input_node_it = nodes_.find(input_name);
                    if (input_node_it != nodes_.end()) {
                        freed_memory += input_node_it->second.getOutputMem();
                    }
                }
            }
        }
        
        // Impact = output_memory - freed_memory
        int impact = node.getOutputMem() - freed_memory;
        node.setImpact(impact);
    }
}

void InputParser::printGraph() const {
    std::cout << "\n=== PARSED GRAPH ===" << std::endl;
    std::cout << "Total Memory: " << total_memory_ << std::endl;
    std::cout << "\nNodes:" << std::endl;
    
    for (const auto& [name, node] : nodes_) {
        std::cout << "  " << name << ": run_mem=" << node.getRunMem() 
                  << ", output_mem=" << node.getOutputMem() 
                  << ", time_cost=" << node.getTimeCost()
                  << ", peak=" << node.getPeak()
                  << ", impact=" << node.getImpact();
        
        if (!node.getInputs().empty()) {
            std::cout << ", inputs=[";
            for (size_t i = 0; i < node.getInputs().size(); ++i) {
                if (i > 0) std::cout << ",";
                std::cout << node.getInputs()[i];
            }
            std::cout << "]";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\nDependencies:" << std::endl;
    for (const auto& [input, consumers] : dependencies_) {
        std::cout << "  " << input << " -> [";
        for (size_t i = 0; i < consumers.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << *std::next(consumers.begin(), i);
        }
        std::cout << "]" << std::endl;
    }
}

std::vector<std::string> InputParser::getRootNodes() const {
    std::vector<std::string> roots;
    for (const auto& [name, node] : nodes_) {
        if (node.getInputs().empty()) {
            roots.push_back(name);
        }
    }
    return roots;
}

std::vector<std::string> InputParser::getLeafNodes() const {
    std::vector<std::string> leaves;
    for (const auto& [name, node] : nodes_) {
        if (successors_.find(name) == successors_.end() || successors_.at(name).empty()) {
            leaves.push_back(name);
        }
    }
    return leaves;
}
