#pragma once

namespace spider_engine {
	struct DefaultPolicy {};

	struct PrioritizeMemoryPolicy {};
	struct PrioritizePerformancePolicy {};
	struct PrioritizeSafetyPolicy {};
	struct NoPriorityPolicy {};

	struct UncopyablePolicy {};
	struct UnmovablePolicy {};

	struct NoThrowPolicy {};
	struct ThrowPolicy {};
};