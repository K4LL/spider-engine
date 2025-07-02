#pragma once

namespace spider_engine {
	struct PrioritizeMemoryPolicy {};
	struct PrioritizePerformancePolicy {};
	struct PrioritizeSafetyPolicy {};
	struct NoPriorityPolicy {};

	struct UncopyablePolicy {};
	struct UnmovablePolicy {};
};