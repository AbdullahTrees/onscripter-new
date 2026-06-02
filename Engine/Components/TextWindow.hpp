/**
 *  TextWindow.hpp
 *  ONScripter-RU
 *
 *  Textbox window compositor.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"

#include "Engine/Graphics/RendererBackend.hpp"

#include <vector>

struct BlitData {
	RenderRect src;
	RenderRect dst;
};

struct Sides {
	float top{0}, right{0}, bottom{0}, left{0};
	Sides(float t, float r, float b, float l)
	    : top(t), right(r), bottom(b), left(l) {}
};

class TextWindowController : public BaseController {
public:
	int ownInit() override;
	int ownDeinit() override;

	TextWindowController()
	    : BaseController(this) {}

	bool usingDynamicTextWindow{false};

	void setWindow(const RenderRect &w) {
		originalWindowSize = w;
	}

	RenderRect mainRegionDimensions{0, 0, 0, 0}; // the area in the texture map occupied by the main region
	float mainRegionExtensionCol{0};

	RenderRect noNameRegionDimensions{0, 0, 0, 0}; // the area in the texture map occupied by the no-name region
	float noNameRegionExtensionCol{0};

	RenderRect nameRegionDimensions{0, 0, 0, 0}; // the area in the texture map occuped by the name region
	float nameRegionExtensionCol{0};

	float nameBoxExtensionCol{0};
	float nameBoxExtensionRow{0};
	float nameBoxDividerCol{0};

	Sides mainRegionPadding{0, 0, 0, 0}, nameBoxPadding{0, 0, 0, 0};

	std::vector<BlitData> getRegions();

	RenderRect getPrintableNameBoxRegion();
	RenderRect getExtendedWindow();
	void updateTextboxExtension(bool smoothly = false);
	int extension{0};

private:
	float previousGoalExtension{0};
	float getRequiredAdditionalHeight(const RenderRect &window);

	RenderRect originalWindowSize{0, 0, 0, 0}; // sentence_font_info.pos, essentially

	std::vector<BlitData> getBottomRegion(const RenderRect &window);
	std::vector<BlitData> getTopRegion(const RenderRect &window);
	std::vector<BlitData> getNameRegion(const RenderRect &window);
	std::vector<BlitData> getNoNameRegion(const RenderRect &window);
	RenderRect getNameBoxRegion(const RenderRect &window);
	RenderRect getPrintableNameBoxRegion(const RenderRect &window);
	RenderRect getExtendedWindow(RenderRect window);
	float getTopOfBottom(const RenderRect &window) {
		return window.y + window.h - mainRegionDimensions.h - mainRegionPadding.bottom;
	}
};

extern TextWindowController wndCtrl;
