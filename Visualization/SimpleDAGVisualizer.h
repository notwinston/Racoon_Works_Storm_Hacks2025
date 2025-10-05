#ifndef SIMPLE_DAG_VISUALIZER_H
#define SIMPLE_DAG_VISUALIZER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include "../include/model.hpp"

// Simple wrapper to add visualization features to the model's Node
class VisualizationNode {
private:
    ::Node node_;
    bool is_recomputed_;
    
public:
    // Default constructor for containers
    VisualizationNode() : is_recomputed_(false) {}
    
    VisualizationNode(const ::Node& node, bool is_recomputed = false) 
        : node_(node), is_recomputed_(is_recomputed) {}
    
    // Delegate all methods to the underlying node
    const std::string& getName() const { return node_.getName(); }
    const std::vector<std::string>& getInputs() const { return node_.getInputs(); }
    int getRunMem() const { return node_.getRunMem(); }
    int getOutputMem() const { return node_.getOutputMem(); }
    int getTimeCost() const { return node_.getTimeCost(); }
    int getPeak() const { return node_.getPeak(); }
    int getImpact() const { return node_.getImpact(); }
    
    // Visualization-specific methods
    bool isRecomputed() const { return is_recomputed_; }
    void setRecomputed(bool recomputed) { is_recomputed_ = recomputed; }
    std::string getDisplayName() const {
        return is_recomputed_ ? node_.getName() + "'" : node_.getName();
    }
};

// Simple DAG Visualizer that works with your scheduler
class SimpleDAGVisualizer {
private:
    std::string output_dir_;
    
public:
    SimpleDAGVisualizer(const std::string& output_dir = "output");
    
    // Main function: Takes ScheduleState and Problem from your algorithm
    void visualizeScheduleState(const ScheduleState& schedule_state, 
                               const Problem& problem,
                               const std::string& filename = "dag_execution");
    
    // Advanced visualization methods for different graph sizes
    void visualizeLargeDAG(const ScheduleState& schedule_state, 
                          const Problem& problem,
                          const std::string& filename = "large_dag");
    
    void visualizeClusteredDAG(const ScheduleState& schedule_state, 
                              const Problem& problem,
                              const std::string& filename = "clustered_dag");
    
    void visualizeHierarchicalDAG(const ScheduleState& schedule_state, 
                                 const Problem& problem,
                                 const std::string& filename = "hierarchical_dag");
    
    // Advanced analysis and console output
    void printExecutionAnalysis(const ScheduleState& schedule_state, const Problem& problem);
    void printRecomputationSummary(const ScheduleState& schedule_state);
    void printMemoryAnalysis(const ScheduleState& schedule_state, const Problem& problem);
    void generateExecutionTimeline(const ScheduleState& schedule_state, 
                                  const Problem& problem,
                                  const std::string& filename = "timeline");
    
    // Helper functions
    void createOutputDirectory();
    std::string formatMemorySize(int bytes);
    bool generatePNGImage(const std::string& dot_filename, const std::string& png_filename);
    
    // Advanced styling and layout helpers
    std::string getNodeColor(const VisualizationNode& node);
    std::string getNodeShape(const VisualizationNode& node);
    std::vector<VisualizationNode> convertScheduleStateToNodes(const ScheduleState& schedule_state, 
                                                               const Problem& problem);
};

#endif // SIMPLE_DAG_VISUALIZER_H
