#pragma once

#include <ostream>
#include <string>

class Synthesizer {
public:
	// Reads an FSM description (format: docs/fsm.net), writes the equivalent
	// gate-level netlist next to it (same name, ".table" extension) and
	// returns the netlist's path. On error, writes to std error and returns "".
	static std::string synthesize(const std::string &path);

	// Same, but writes the netlist to `out` instead of a file. Nothing is
	// written on error. Returns false on error.
	static bool synthesize(const std::string &path, std::ostream &out);
};
