#include "SimpleDAGVisualizer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <map>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

SimpleDAGVisualizer::SimpleDAGVisualizer(const std::string& output_dir) 
    : output_dir_(output_dir) {
    createOutputDirectory();
}

// ============================================================================
// MAIN VISUALIZATION FUNCTION
// ============================================================================

void SimpleDAGVisualizer::visualizeScheduleState(const ScheduleState& schedule_state, 
                                                 const Problem& problem,
                                                 const std::string& filename) {
    std::cout << "🎨 Creating DAG visualization from ScheduleState..." << std::endl;
    
    // Convert ScheduleState to vector<VisualizationNode>
    std::vector<VisualizationNode> execution_order;
    
    for (size_t i = 0; i < schedule_state.execution_order.size(); ++i) {
        const std::string& node_name = schedule_state.execution_order[i];
        bool is_recomputed = (i < schedule_state.recompute_flags.size()) ? schedule_state.recompute_flags[i] : false;
        
        // Find the original node in the problem
        auto it = problem.nodes.find(node_name);
        if (it != problem.nodes.end()) {
            const ::Node& original_node = it->second;
            VisualizationNode viz_node(original_node, is_recomputed);
            execution_order.push_back(viz_node);
        }
    }
    
    // Generate DOT file
    std::string dot_filepath = output_dir_ + "/" + filename + ".dot";
    std::ofstream dot_file(dot_filepath);
    
    if (!dot_file.is_open()) {
        std::cerr << "Error: Cannot create DOT file: " << dot_filepath << std::endl;
        return;
    }
    
    // Generate DOT content
    dot_file << "digraph ExecutionOrder {\n";
    dot_file << "    rankdir=TB;\n";
    dot_file << "    splines=false;\n";
    dot_file << "    nodesep=0.8;\n";
    dot_file << "    ranksep=1.2;\n";
    dot_file << "    \n";
    dot_file << "    node [\n";
    dot_file << "        shape=box,\n";
    dot_file << "        style=filled,\n";
    dot_file << "        fontname=\"Arial Bold\",\n";
    dot_file << "        fontsize=14,\n";
    dot_file << "        fontcolor=white,\n";
    dot_file << "        width=1.5,\n";
    dot_file << "        height=1.0,\n";
    dot_file << "        fixedsize=true\n";
    dot_file << "    ];\n";
    dot_file << "    \n";
    dot_file << "    edge [\n";
    dot_file << "        fontname=\"Arial\",\n";
    dot_file << "        fontsize=10,\n";
    dot_file << "        color=\"#2c3e50\",\n";
    dot_file << "        penwidth=2.5,\n";
    dot_file << "        arrowsize=1.2\n";
    dot_file << "    ];\n\n";
    
    // Add nodes
    for (const auto& node : execution_order) {
        std::string node_id = node.getDisplayName();
        dot_file << "    \"" << node_id << "\" [\n";
        dot_file << "        label=\"" << node_id << "\\n";
        dot_file << "Mem: " << formatMemorySize(node.getRunMem()) << "\\n";
        dot_file << "Out: " << formatMemorySize(node.getOutputMem()) << "\\n";
        dot_file << "Time: " << node.getTimeCost() << "\",\n";
        dot_file << "        fillcolor=\"" << (node.isRecomputed() ? "#e74c3c" : "#3498db") << "\",\n";
        dot_file << "        shape=\"" << (node.isRecomputed() ? "ellipse" : "box") << "\",\n";
        dot_file << "        penwidth=3,\n";
        dot_file << "        pencolor=\"" << (node.isRecomputed() ? "#e74c3c" : "#2c3e50") << "\"\n";
        dot_file << "    ];\n";
    }
    
    // Add edges
    for (const auto& node : execution_order) {
        for (const auto& input : node.getInputs()) {
            // Find the most recent execution of the input
            std::string input_id = input;
            for (const auto& other_node : execution_order) {
                if (other_node.getName() == input && other_node.isRecomputed()) {
                    input_id = other_node.getDisplayName();
                    break;
                } else if (other_node.getName() == input && !other_node.isRecomputed()) {
                    input_id = other_node.getDisplayName();
                }
            }
            
            dot_file << "    \"" << input_id << "\" -> \"" << node.getDisplayName() << "\" [\n";
            dot_file << "        color=\"" << (node.isRecomputed() ? "#e74c3c" : "#3498db") << "\",\n";
            dot_file << "        penwidth=" << (node.isRecomputed() ? "3" : "2") << "\n";
            dot_file << "    ];\n";
        }
    }
    
    dot_file << "}\n";
    dot_file.close();
    
    std::cout << "✓ DOT file generated: " << dot_filepath << std::endl;
    
    // Generate summary
    std::string summary_filepath = output_dir_ + "/" + filename + "_summary.txt";
    std::ofstream summary_file(summary_filepath);
    
    if (summary_file.is_open()) {
        summary_file << "=== DAG EXECUTION ORDER SUMMARY ===\n\n";
        summary_file << "EXECUTION SEQUENCE:\n";
        summary_file << std::string(40, '-') << "\n";
        for (size_t i = 0; i < execution_order.size(); ++i) {
            const auto& node = execution_order[i];
            summary_file << (i + 1) << ". " << node.getDisplayName();
            if (node.isRecomputed()) {
                summary_file << " (RECOMPUTED)";
            }
            summary_file << "\n";
        }
        
        summary_file << "\n\nRECOMPUTATION ANALYSIS:\n";
        summary_file << std::string(40, '-') << "\n";
        
        std::unordered_map<std::string, int> recomputation_count;
        for (const auto& node : execution_order) {
            recomputation_count[node.getName()]++;
        }
        
        for (const auto& [name, count] : recomputation_count) {
            if (count > 1) {
                summary_file << name << ": " << count << " executions (recomputed " << (count - 1) << " times)\n";
            }
        }
        
        summary_file << "\n\nMEMORY ANALYSIS:\n";
        summary_file << std::string(40, '-') << "\n";
        
        int total_run_mem = 0, total_output_mem = 0, total_time = 0;
        for (const auto& node : execution_order) {
            total_run_mem += node.getRunMem();
            total_output_mem += node.getOutputMem();
            total_time += node.getTimeCost();
        }
        
        summary_file << "Total Run Memory: " << formatMemorySize(total_run_mem) << "\n";
        summary_file << "Total Output Memory: " << formatMemorySize(total_output_mem) << "\n";
        summary_file << "Total Execution Time: " << total_time << " units\n";
        summary_file << "Peak Memory: " << formatMemorySize(schedule_state.memory_peak) << "\n";
        summary_file << "Total Time: " << schedule_state.total_time << " units\n";
        
        summary_file.close();
        std::cout << "✓ Summary generated: " << summary_filepath << std::endl;
    }
    
    // Automatically generate PNG image
    std::string png_filepath = output_dir_ + "/" + filename + ".png";
    generatePNGImage(dot_filepath, png_filepath);
    
    std::cout << "\n✅ Visualization complete!" << std::endl;
    std::cout << "📁 Generated files in '" << output_dir_ << "' folder:" << std::endl;
    std::cout << "   • " << filename << ".dot - Graphviz file" << std::endl;
    std::cout << "   • " << filename << "_summary.txt - Execution summary" << std::endl;
    std::cout << "   • " << filename << ".png - Visual image (auto-generated)" << std::endl;
    
    // Generate additional analysis
    printExecutionAnalysis(schedule_state, problem);
    printRecomputationSummary(schedule_state);
    printMemoryAnalysis(schedule_state, problem);
    generateExecutionTimeline(schedule_state, problem, filename + "_timeline");
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void SimpleDAGVisualizer::createOutputDirectory() {
    try {
        std::filesystem::create_directories(output_dir_);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not create output directory: " << e.what() << std::endl;
        output_dir_ = ".";  // Fallback to current directory
    }
}

std::string SimpleDAGVisualizer::formatMemorySize(int bytes) {
    if (bytes >= 1024) {
        return std::to_string(bytes / 1024) + "K";
    }
    return std::to_string(bytes);
}

bool SimpleDAGVisualizer::generatePNGImage(const std::string& dot_filename, const std::string& png_filename) {
    // Try different Graphviz paths for Windows
    std::vector<std::string> graphviz_paths = {
        "C:\\Program Files\\Graphviz\\bin\\dot.exe",
        "C:\\Program Files (x86)\\Graphviz\\bin\\dot.exe",
        "dot.exe",  // If in PATH
        "dot"       // Alternative command
    };
    
    std::string dot_command;
    bool found_graphviz = false;
    
    // Find the first available Graphviz installation
    for (const auto& path : graphviz_paths) {
        if (path == "dot.exe" || path == "dot") {
            // Try system PATH
            dot_command = path;
            found_graphviz = true;
            break;
        } else {
            // Check if file exists (simplified check)
            std::ifstream test_file(path);
            if (test_file.good()) {
                dot_command = path;
                found_graphviz = true;
                break;
            }
            test_file.close();
        }
    }
    
    if (!found_graphviz) {
        std::cout << "⚠️  Graphviz not found. Please install Graphviz and add it to PATH." << std::endl;
        std::cout << "   Download from: https://graphviz.org/download/" << std::endl;
        std::cout << "   Manual command: " << dot_command << " -Tpng " << dot_filename << " -o " << png_filename << std::endl;
        return false;
    }
    
    // Build the command - use PowerShell call operator for paths with spaces
    std::string command = "powershell.exe -Command \"& '" + dot_command + "' -Tpng '" + dot_filename + "' -o '" + png_filename + "'\"";
    
    // Execute the command
    int result = std::system(command.c_str());
    
    if (result == 0) {
        std::cout << "✓ PNG image generated: " << png_filename << std::endl;
        return true;
    } else {
        std::cout << "❌ Failed to generate PNG image. Command: " << command << std::endl;
        return false;
    }
}

// ============================================================================
// ADVANCED VISUALIZATION METHODS
// ============================================================================

void SimpleDAGVisualizer::visualizeLargeDAG(const ScheduleState& schedule_state, 
                                            const Problem& problem,
                                            const std::string& filename) {
    std::cout << "🎨 Creating large DAG visualization..." << std::endl;
    
    std::vector<VisualizationNode> execution_order = convertScheduleStateToNodes(schedule_state, problem);
    
    std::string dot_filepath = output_dir_ + "/" + filename + ".dot";
    std::ofstream dot_file(dot_filepath);
    
    if (!dot_file.is_open()) {
        std::cerr << "Error: Cannot create DOT file: " << dot_filepath << std::endl;
        return;
    }
    
    // For large graphs, use a more organized layout
    dot_file << "digraph G {\n";
    dot_file << "    rankdir=TB;\n";
    dot_file << "    splines=false;\n";
    dot_file << "    nodesep=0.4;\n";
    dot_file << "    ranksep=0.6;\n";
    dot_file << "    compound=true;\n";
    dot_file << "    newrank=true;\n";
    dot_file << "    \n";
    dot_file << "    node [\n";
    dot_file << "        style=filled,\n";
    dot_file << "        fontname=\"Arial\",\n";
    dot_file << "        fontsize=8,\n";
    dot_file << "        fontcolor=white,\n";
    dot_file << "        width=0.8,\n";
    dot_file << "        height=0.5,\n";
    dot_file << "        fixedsize=true\n";
    dot_file << "    ];\n";
    dot_file << "    \n";
    dot_file << "    edge [\n";
    dot_file << "        fontname=\"Arial\",\n";
    dot_file << "        fontsize=6,\n";
    dot_file << "        penwidth=1.0,\n";
    dot_file << "        arrowsize=0.5\n";
    dot_file << "    ];\n\n";
    
    // Create tree structure for large DAG
    std::map<int, std::vector<std::string>> tree_layers;
    std::map<std::string, int> node_layers;
    std::unordered_map<std::string, VisualizationNode> node_map;
    
    // Build node map
    for (const auto& node : execution_order) {
        std::string node_id = node.getDisplayName();
        node_map[node_id] = node;
    }
    
    // Create tree layers based on dependencies
    for (const auto& node : execution_order) {
        std::string node_id = node.getDisplayName();
        
        // Calculate tree layer based on dependencies
        int layer = 0;
        if (!node.getInputs().empty()) {
            // Find the maximum layer of input nodes
            for (const auto& input : node.getInputs()) {
                for (const auto& [key, other_node] : node_map) {
                    if (other_node.getName() == input && node_layers.find(key) != node_layers.end()) {
                        layer = std::max(layer, node_layers[key] + 1);
                    }
                }
            }
        }
        
        // For recomputed nodes, place them in a special layer
        if (node.isRecomputed()) {
            layer = 1000 + node_layers.size();
        }
        
        node_layers[node_id] = layer;
        tree_layers[layer].push_back(node_id);
    }

    // Add nodes with layer grouping
    dot_file << "    // Node definitions organized by tree layers\n";
    for (const auto& [layer, nodes] : tree_layers) {
        dot_file << "    // Tree Layer " << layer << "\n";
        dot_file << "    { rank=same;\n";
        
        for (const auto& node_id : nodes) {
            const VisualizationNode& node = node_map[node_id];
            dot_file << "        \"" << node_id << "\" [\n";
            dot_file << "            label=\"" << node_id << "\\n" << formatMemorySize(node.getRunMem()) << "\",\n";
            dot_file << "            shape=" << getNodeShape(node) << ",\n";
            dot_file << "            fillcolor=\"" << getNodeColor(node) << "\",\n";
            dot_file << "            pencolor=\"#2c3e50\",\n";
            dot_file << "            tooltip=\"" << node_id << "\\nRun: " << node.getRunMem() 
                    << "\\nTime: " << node.getTimeCost() << "s"
                    << (node.isRecomputed() ? "\\nRECOMPUTED" : "") << "\"\n";
            dot_file << "        ];\n";
        }
        
        dot_file << "    }\n\n";
    }

    // Add edges
    dot_file << "    // Edge definitions\n";
    for (const auto& node : execution_order) {
        std::string node_id = node.getDisplayName();
        
        for (const auto& input : node.getInputs()) {
            std::string input_id = input;
            bool found_recomputed = false;
            
            // First, try to find a recomputed version of this input that comes before current node
            for (const auto& [key, other_node] : node_map) {
                if (other_node.getName() == input && other_node.isRecomputed()) {
                    input_id = key;
                    found_recomputed = true;
                    break;
                }
            }
            
            // If no recomputed version found, use the original
            if (!found_recomputed) {
                for (const auto& [key, other_node] : node_map) {
                    if (other_node.getName() == input && !other_node.isRecomputed()) {
                        input_id = key;
                        break;
                    }
                }
            }
            
            dot_file << "    \"" << input_id << "\" -> \"" << node_id << "\" [\n";
            dot_file << "        color=\"" << (found_recomputed ? "#e74c3c" : "#3498db") << "\",\n";
            dot_file << "        style=\"solid\"\n";
            dot_file << "    ];\n";
        }
    }
    
    dot_file << "}\n";
    dot_file.close();
    
    std::cout << "✓ Large DAG DOT file generated: " << dot_filepath << std::endl;
    
    // Generate PNG
    std::string png_filepath = output_dir_ + "/" + filename + ".png";
    generatePNGImage(dot_filepath, png_filepath);
}

void SimpleDAGVisualizer::visualizeClusteredDAG(const ScheduleState& schedule_state, 
                                                const Problem& problem,
                                                const std::string& filename) {
    std::cout << "🎨 Creating clustered DAG visualization..." << std::endl;
    
    std::vector<VisualizationNode> execution_order = convertScheduleStateToNodes(schedule_state, problem);
    
    std::string dot_filepath = output_dir_ + "/" + filename + ".dot";
    std::ofstream dot_file(dot_filepath);
    
    if (!dot_file.is_open()) {
        std::cerr << "Error: Cannot create DOT file: " << dot_filepath << std::endl;
        return;
    }
    
    // For very large graphs, use clustering approach
    dot_file << "digraph G {\n";
    dot_file << "    rankdir=TB;\n";
    dot_file << "    splines=false;\n";
    dot_file << "    nodesep=0.3;\n";
    dot_file << "    ranksep=0.5;\n";
    dot_file << "    compound=true;\n";
    dot_file << "    newrank=true;\n";
    dot_file << "    \n";
    dot_file << "    node [\n";
    dot_file << "        style=filled,\n";
    dot_file << "        fontname=\"Arial\",\n";
    dot_file << "        fontsize=6,\n";
    dot_file << "        fontcolor=white,\n";
    dot_file << "        width=0.6,\n";
    dot_file << "        height=0.4,\n";
    dot_file << "        fixedsize=true\n";
    dot_file << "    ];\n";
    dot_file << "    \n";
    dot_file << "    edge [\n";
    dot_file << "        fontname=\"Arial\",\n";
    dot_file << "        fontsize=4,\n";
    dot_file << "        penwidth=0.8,\n";
    dot_file << "        arrowsize=0.4\n";
    dot_file << "    ];\n\n";

    // Group nodes into clusters based on execution phases
    std::vector<std::vector<VisualizationNode>> clusters;
    std::vector<VisualizationNode> current_cluster;
    int cluster_size = 15; // Max nodes per cluster
    
    for (size_t i = 0; i < execution_order.size(); ++i) {
        current_cluster.push_back(execution_order[i]);
        if (current_cluster.size() >= cluster_size || i == execution_order.size() - 1) {
            clusters.push_back(current_cluster);
            current_cluster.clear();
        }
    }

    // Generate clusters
    for (size_t cluster_id = 0; cluster_id < clusters.size(); ++cluster_id) {
        const auto& cluster = clusters[cluster_id];
        dot_file << "    subgraph cluster_" << cluster_id << " {\n";
        dot_file << "        label=\"Phase " << (cluster_id + 1) << " (" << cluster.size() << " nodes)\";\n";
        dot_file << "        style=filled;\n";
        dot_file << "        fillcolor=\"#f8f9fa\";\n";
        dot_file << "        fontsize=8;\n";
        dot_file << "        \n";
        
        // Add nodes to cluster
        for (const auto& node : cluster) {
            std::string node_id = node.getDisplayName();
            
            dot_file << "        \"" << node_id << "\" [\n";
            dot_file << "            label=\"" << node_id << "\",\n";
            dot_file << "            shape=" << getNodeShape(node) << ",\n";
            dot_file << "            fillcolor=\"" << getNodeColor(node) << "\",\n";
            dot_file << "            pencolor=\"" << (node.isRecomputed() ? "#e74c3c" : "#2c3e50") << "\",\n";
            dot_file << "            tooltip=\"" << node_id << "\\nRun: " << node.getRunMem() 
                    << "\\nTime: " << node.getTimeCost() << "s"
                    << (node.isRecomputed() ? "\\nRECOMPUTED" : "") << "\"\n";
            dot_file << "        ];\n";
        }
        
        dot_file << "    }\n\n";
    }

    // Add simplified edges (only between clusters and key dependencies)
    dot_file << "    // Simplified edge definitions\n";
    for (const auto& node : execution_order) {
        std::string node_id = node.getDisplayName();
        
        for (const auto& input : node.getInputs()) {
            // Only show edges for recomputed nodes or cross-cluster dependencies
            bool is_recomputed = node.isRecomputed();
            bool is_cross_cluster = false;
            
            // Check if this is a cross-cluster dependency
            for (const auto& cluster : clusters) {
                bool input_in_cluster = false;
                bool node_in_cluster = false;
                for (const auto& cluster_node : cluster) {
                    if (cluster_node.getName() == input) input_in_cluster = true;
                    if (cluster_node.getName() == node.getName()) node_in_cluster = true;
                }
                if (input_in_cluster != node_in_cluster) {
                    is_cross_cluster = true;
                    break;
                }
            }
            
            if (is_recomputed || is_cross_cluster) {
                std::string input_id = input;
                // Find the most recent execution of the input
                for (const auto& other_node : execution_order) {
                    if (other_node.getName() == input && other_node.isRecomputed()) {
                        input_id = other_node.getDisplayName();
                        break;
                    }
                }
                
                dot_file << "    \"" << input_id << "\" -> \"" << node_id << "\" [\n";
                dot_file << "        color=\"" << (is_recomputed ? "#e74c3c" : "#3498db") << "\",\n";
                dot_file << "        style=\"solid\"\n";
                dot_file << "    ];\n";
            }
        }
    }
    
    dot_file << "}\n";
    dot_file.close();
    
    std::cout << "✓ Clustered DAG DOT file generated: " << dot_filepath << std::endl;
    
    // Generate PNG
    std::string png_filepath = output_dir_ + "/" + filename + ".png";
    generatePNGImage(dot_filepath, png_filepath);
}

void SimpleDAGVisualizer::visualizeHierarchicalDAG(const ScheduleState& schedule_state, 
                                                   const Problem& problem,
                                                   const std::string& filename) {
    std::cout << "🎨 Creating hierarchical DAG visualization..." << std::endl;
    
    std::vector<VisualizationNode> execution_order = convertScheduleStateToNodes(schedule_state, problem);
    
    std::string dot_filepath = output_dir_ + "/" + filename + ".dot";
    std::ofstream dot_file(dot_filepath);
    
    if (!dot_file.is_open()) {
        std::cerr << "Error: Cannot create DOT file: " << dot_filepath << std::endl;
        return;
    }
    
    // For huge graphs, create a high-level summary view
    dot_file << "digraph G {\n";
    dot_file << "    rankdir=TB;\n";
    dot_file << "    splines=false;\n";
    dot_file << "    nodesep=1.0;\n";
    dot_file << "    ranksep=1.5;\n";
    dot_file << "    \n";
    dot_file << "    node [\n";
    dot_file << "        style=filled,\n";
    dot_file << "        fontname=\"Arial Bold\",\n";
    dot_file << "        fontsize=12,\n";
    dot_file << "        fontcolor=white,\n";
    dot_file << "        width=2.0,\n";
    dot_file << "        height=1.0,\n";
    dot_file << "        fixedsize=true\n";
    dot_file << "    ];\n";
    dot_file << "    \n";
    dot_file << "    edge [\n";
    dot_file << "        fontname=\"Arial\",\n";
    dot_file << "        fontsize=10,\n";
    dot_file << "        penwidth=3.0,\n";
    dot_file << "        arrowsize=1.0\n";
    dot_file << "    ];\n\n";

    // Create high-level phases
    std::vector<std::string> phases = {"INPUT", "PROCESSING", "RECOMPUTATION", "OUTPUT"};
    std::map<std::string, int> phase_counts;
    std::map<std::string, int> phase_recomputed;
    
    // Categorize nodes into phases
    for (const auto& node : execution_order) {
        if (node.getInputs().empty()) {
            phase_counts["INPUT"]++;
        } else if (node.isRecomputed()) {
            phase_counts["RECOMPUTATION"]++;
            phase_recomputed["RECOMPUTATION"]++;
        } else if (node.getName().find("Return") != std::string::npos) {
            phase_counts["OUTPUT"]++;
        } else {
            phase_counts["PROCESSING"]++;
        }
    }

    // Create phase nodes
    for (const auto& phase : phases) {
        if (phase_counts[phase] > 0) {
            dot_file << "    \"" << phase << "\" [\n";
            dot_file << "        label=\"" << phase << "\\n" << phase_counts[phase] << " nodes";
            if (phase_recomputed[phase] > 0) {
                dot_file << "\\n(" << phase_recomputed[phase] << " recomputed)";
            }
            dot_file << "\",\n";
            dot_file << "        fillcolor=\"" << (phase == "RECOMPUTATION" ? "#e74c3c" : 
                                                   phase == "INPUT" ? "#2ecc71" :
                                                   phase == "OUTPUT" ? "#f39c12" : "#3498db") << "\",\n";
            dot_file << "        shape=\"box\",\n";
            dot_file << "        tooltip=\"" << phase << " phase with " << phase_counts[phase] << " nodes\"\n";
            dot_file << "    ];\n";
        }
    }

    // Add flow between phases
    if (phase_counts["INPUT"] > 0 && phase_counts["PROCESSING"] > 0) {
        dot_file << "    \"INPUT\" -> \"PROCESSING\" [color=\"#2ecc71\", penwidth=3];\n";
    }
    if (phase_counts["PROCESSING"] > 0 && phase_counts["RECOMPUTATION"] > 0) {
        dot_file << "    \"PROCESSING\" -> \"RECOMPUTATION\" [color=\"#e74c3c\", penwidth=3];\n";
    }
    if (phase_counts["RECOMPUTATION"] > 0 && phase_counts["OUTPUT"] > 0) {
        dot_file << "    \"RECOMPUTATION\" -> \"OUTPUT\" [color=\"#f39c12\", penwidth=3];\n";
    } else if (phase_counts["PROCESSING"] > 0 && phase_counts["OUTPUT"] > 0) {
        dot_file << "    \"PROCESSING\" -> \"OUTPUT\" [color=\"#3498db\", penwidth=3];\n";
    }
    
    dot_file << "}\n";
    dot_file.close();
    
    std::cout << "✓ Hierarchical DAG DOT file generated: " << dot_filepath << std::endl;
    
    // Generate PNG
    std::string png_filepath = output_dir_ + "/" + filename + ".png";
    generatePNGImage(dot_filepath, png_filepath);
}

// ============================================================================
// ANALYSIS AND CONSOLE OUTPUT METHODS
// ============================================================================

void SimpleDAGVisualizer::printExecutionAnalysis(const ScheduleState& schedule_state, const Problem& problem) {
    std::cout << "\n=== EXECUTION ORDER ANALYSIS ===" << std::endl;
    std::cout << "Total nodes executed: " << schedule_state.execution_order.size() << std::endl;
    
    std::cout << "\nExecution sequence:" << std::endl;
    for (size_t i = 0; i < schedule_state.execution_order.size(); ++i) {
        std::cout << (i + 1) << ". " << schedule_state.execution_order[i];
        bool is_recomputed = (i < schedule_state.recompute_flags.size()) ? schedule_state.recompute_flags[i] : false;
        if (is_recomputed) {
            std::cout << " (RECOMPUTED)";
        }
        std::cout << std::endl;
    }
}

void SimpleDAGVisualizer::printRecomputationSummary(const ScheduleState& schedule_state) {
    std::cout << "\n=== RECOMPUTATION SUMMARY ===" << std::endl;
    
    std::unordered_map<std::string, int> recomputation_count;
    for (size_t i = 0; i < schedule_state.execution_order.size(); ++i) {
        const std::string& name = schedule_state.execution_order[i];
        bool is_recomputed = (i < schedule_state.recompute_flags.size()) ? schedule_state.recompute_flags[i] : false;
        if (is_recomputed) {
            recomputation_count[name]++;
        }
    }
    
    bool has_recomputations = false;
    for (const auto& [name, count] : recomputation_count) {
        if (count > 0) {
            std::cout << name << ": " << count << " recomputations" << std::endl;
            has_recomputations = true;
        }
    }
    
    if (!has_recomputations) {
        std::cout << "No nodes were recomputed." << std::endl;
    }
}

void SimpleDAGVisualizer::printMemoryAnalysis(const ScheduleState& schedule_state, const Problem& problem) {
    std::cout << "\n=== MEMORY ANALYSIS ===" << std::endl;
    
    int total_run_mem = 0, total_output_mem = 0, total_time = 0;
    for (const auto& [name, node] : problem.nodes) {
        total_run_mem += node.getRunMem();
        total_output_mem += node.getOutputMem();
        total_time += node.getTimeCost();
    }
    
    std::cout << "Total Run Memory: " << formatMemorySize(total_run_mem) << std::endl;
    std::cout << "Total Output Memory: " << formatMemorySize(total_output_mem) << std::endl;
    std::cout << "Total Execution Time: " << total_time << " units" << std::endl;
    std::cout << "Peak Memory Usage: " << formatMemorySize(schedule_state.memory_peak) << std::endl;
    std::cout << "Actual Total Time: " << schedule_state.total_time << " units" << std::endl;
}

void SimpleDAGVisualizer::generateExecutionTimeline(const ScheduleState& schedule_state, 
                                                   const Problem& problem,
                                                   const std::string& filename) {
    std::string timeline_filepath = output_dir_ + "/" + filename + ".txt";
    std::ofstream timeline_file(timeline_filepath);
    
    if (!timeline_file.is_open()) {
        std::cerr << "Error: Cannot create timeline file: " << timeline_filepath << std::endl;
        return;
    }
    
    timeline_file << "=== EXECUTION TIMELINE ===\n\n";
    
    int current_time = 0;
    int current_memory = 0;
    int peak_memory = 0;
    
    timeline_file << "Step-by-step execution:\n";
    timeline_file << std::string(60, '-') << "\n";
    timeline_file << std::left << std::setw(8) << "Time" 
                 << std::setw(15) << "Node" 
                 << std::setw(12) << "Run Mem" 
                 << std::setw(12) << "Output Mem" 
                 << std::setw(15) << "Total Memory" 
                 << "Status\n";
    timeline_file << std::string(60, '-') << "\n";
    
    for (size_t i = 0; i < schedule_state.execution_order.size(); ++i) {
        const std::string& node_name = schedule_state.execution_order[i];
        bool is_recomputed = (i < schedule_state.recompute_flags.size()) ? schedule_state.recompute_flags[i] : false;
        
        // Find the node in the problem
        auto it = problem.nodes.find(node_name);
        if (it != problem.nodes.end()) {
            const ::Node& node = it->second;
            
            // Update memory tracking
            current_memory += node.getOutputMem();
            peak_memory = std::max(peak_memory, current_memory + node.getRunMem());
            
            timeline_file << std::left << std::setw(8) << current_time
                         << std::setw(15) << node_name
                         << std::setw(12) << formatMemorySize(node.getRunMem())
                         << std::setw(12) << formatMemorySize(node.getOutputMem())
                         << std::setw(15) << formatMemorySize(peak_memory);
            
            if (is_recomputed) {
                timeline_file << "RECOMPUTED";
            } else {
                timeline_file << "FIRST EXECUTION";
            }
            timeline_file << "\n";
            
            current_time += node.getTimeCost();
        }
    }
    
    timeline_file << "\n\nSUMMARY:\n";
    timeline_file << std::string(30, '-') << "\n";
    timeline_file << "Total Execution Time: " << current_time << " units\n";
    timeline_file << "Peak Memory Usage: " << formatMemorySize(peak_memory) << "\n";
    timeline_file << "Final Memory Usage: " << formatMemorySize(current_memory) << "\n";
    
    timeline_file.close();
    std::cout << "✓ Execution timeline generated: " << timeline_filepath << std::endl;
}

// ============================================================================
// ADVANCED HELPER FUNCTIONS
// ============================================================================

std::string SimpleDAGVisualizer::getNodeColor(const VisualizationNode& node) {
    if (node.isRecomputed()) {
        return "#e74c3c";  // Red for recomputed nodes
    }
    
    // Color based on memory usage
    int peak = std::max(node.getRunMem(), node.getOutputMem());
    if (peak > 150) return "#f39c12";      // Orange
    if (peak > 100) return "#f1c40f";      // Yellow
    if (peak > 50) return "#2ecc71";       // Green
    return "#3498db";                      // Blue
}

std::string SimpleDAGVisualizer::getNodeShape(const VisualizationNode& node) {
    return node.isRecomputed() ? "ellipse" : "box";
}

std::vector<VisualizationNode> SimpleDAGVisualizer::convertScheduleStateToNodes(const ScheduleState& schedule_state, 
                                                                               const Problem& problem) {
    std::vector<VisualizationNode> execution_order;
    
    for (size_t i = 0; i < schedule_state.execution_order.size(); ++i) {
        const std::string& node_name = schedule_state.execution_order[i];
        bool is_recomputed = (i < schedule_state.recompute_flags.size()) ? schedule_state.recompute_flags[i] : false;
        
        // Find the original node in the problem
        auto it = problem.nodes.find(node_name);
        if (it != problem.nodes.end()) {
            const ::Node& original_node = it->second;
            VisualizationNode viz_node(original_node, is_recomputed);
            execution_order.push_back(viz_node);
        }
    }
    
    return execution_order;
}
