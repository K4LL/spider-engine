#pragma once

namespace spider_engine::hub {
	class IPage {
	public:
		virtual ~IPage() = default;

		virtual void draw() = 0;
	};

	enum class HubPage {
		MAIN = 1,
	};
}