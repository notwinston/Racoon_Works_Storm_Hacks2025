#ifndef NODE_HPP
#define NODE_HPP

#include <string>
#include <vector>

class Node {
private:
    std::string name_; // The name of the operator node, such as Add, Mul, etc.
    std::vector<Node> inputs_; // All input nodes of the operator node
    int run_mem_; // Memory required for the operator's computation process
    int output_mem_; // Memory occupied by the operator's computation result
    int time_cost_; // Time taken for the operator's computation

public:
    Node()
    : name_(""), run_mem_(0), output_mem_(0), time_cost_(0) {}

    // Parameterized constructor
    Node(const std::string& name,
         const std::vector<Node>& inputs,
         int run_mem,
         int output_mem,
         int time_cost)
    : name_(name),
      inputs_(inputs),
      run_mem_(run_mem),
      output_mem_(output_mem),
      time_cost_(time_cost) {}

    // Getters
    const std::string& getName() const { return name_; }
    const std::vector<Node>& getInputs() const { return inputs_; }
    int getRunMem() const { return run_mem_; }
    int getOutputMem() const { return output_mem_; }
    int getTimeCost() const { return time_cost_; }
};

#endif // NODE_HPP