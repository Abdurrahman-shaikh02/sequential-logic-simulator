#pragma once

#include <string>
#include <vector>
#include <memory>

#include "core/Node.hpp"
#include "simulation/OperationQueue.hpp"

class Simulator {
	//the simulator instance we use can be a global object so both queues and nodes can be global...
	static OperationQueue combination_queue;
	static OperationQueue sequential_queue;

    static std::vector<std::unique_ptr<Node>> nodes;

	static int primary_input_count;
	static int primary_output_count;

    // parsers and synthesizer entry points
	static int parse(const std::string &path);

	static std::string synthesize(const std::string &path);

	// Two-phase simulation driver
    static void run();

    // Reset simulation state
    static void clear();
};
