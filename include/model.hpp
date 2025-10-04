#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class Node {
private:
    std::string name_;
    std::vector<std::string> input_names_;
    int run_mem_;
    int output_mem_;
    int time_cost_;
    int peak_;
    int impact_;
public:
    Node() : run_mem_(0), output_mem_(0), time_cost_(0), peak_(0), impact_(0) {}
    Node(std::string name, std::vector<std::string> inputs, int run_mem, int output_mem, int time_cost)
        : name_(std::move(name)), input_names_(std::move(inputs)), run_mem_(run_mem), output_mem_(output_mem), time_cost_(time_cost) {
        peak_ = std::max(run_mem_, output_mem_);
        impact_ = output_mem_;
    }
    const std::string& getName() const { return name_; }
    const std::vector<std::string>& getInputs() const { return input_names_; }
    int getRunMem() const { return run_mem_; }
    int getOutputMem() const { return output_mem_; }
    int getTimeCost() const { return time_cost_; }
    int getPeak() const { return peak_; }
    int getImpact() const { return impact_; }
    void setImpact(int impact) { impact_ = impact; }
};

struct ScheduleState {
    std::vector<std::string> execution_order;
    std::vector<bool> recompute_flags; // true if this step is a recomputation of a previously executed node
    int current_memory{0};
    int memory_peak{0};
    int total_time{0};
    std::unordered_set<std::string> computed;
    std::unordered_map<std::string, int> output_memory;
};

struct Problem {
    long total_memory{0};
    std::unordered_map<std::string, Node> nodes;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependencies; // input -> consumers
    std::unordered_map<std::string, std::vector<std::string>> successors; // node -> consumers list
};


