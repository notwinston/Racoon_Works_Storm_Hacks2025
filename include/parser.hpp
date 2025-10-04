#pragma once

#include "model.hpp"
#include <string>
#include <vector>
#include <istream>
/**
 * ParsedNodeSpec struct - Represents a parsed node specification from the txt file
 * 
 * Attributes:
 * - name: The name of the node
 * - run_mem: Memory required during computation
 * - output_mem: Memory occupied by the result
 * - time_cost: Time required to execute the node
 * - inputs: Names of input nodes required for each operator (node)
 */
struct ParsedNodeSpec {
    std::string name;
    int run_mem{0};
    int output_mem{0};
    int time_cost{0};
    std::vector<std::string> inputs;
};

/**
 * parseExamplesFormat function - Parses the examples format from the txt file
 * 
 * Attributes:
 * - in: The input stream
 * - total_memory: The total memory available
 * - nodes_out: The nodes out
 * - error: The error
 */
bool parseExamplesFormat(std::istream& in, long& total_memory,
                         std::vector<ParsedNodeSpec>& nodes_out,
                         std::string& error);

/**
 * parseSimpleFormat function - Parses the simple format from the txt file
 * 
 * Attributes:
 * - in: The input stream
 * - total_memory: The total memory available
 * - nodes_out: The nodes out
 * - error: The error
 */
bool parseSimpleFormat(std::istream& in, long& total_memory,
                       std::vector<ParsedNodeSpec>& nodes_out,
                       std::string& error);

/**
 * buildProblem function - Builds the problem from the parsed node specifications
 * 
 * Attributes:
 * - total_memory: The total memory available
 * - specs: The parsed node specifications
 */
Problem buildProblem(long total_memory, const std::vector<ParsedNodeSpec>& specs);


