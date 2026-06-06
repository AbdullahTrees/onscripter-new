/**
 *  TextWindow.cpp
 *  ONScripter-RU
 *
 *  Textbox window compositor.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/TextWindow.hpp"
#include "Engine/Components/Dialogue.hpp"
#include "Support/Clock.hpp"

#include "Engine/Graphics/RendererBackend.hpp"

TextWindowController wndCtrl;

static BlitData extendDown(BlitData b, float y) {
	b.src.y += b.src.h - 1;
	b.src.h = 1;
	b.dst.y += b.dst.h;
	b.dst.h = y;
	return b;
}

static void appendThreeSplit(BlitRegionList &blits, const BlitData &b, float x) {
	BlitData l, m, r;
	l.src = {b.src.x, b.src.y, x, b.src.h};
	m.src = {b.src.x + x, b.src.y, 1, b.src.h};
	r.src = {b.src.x + x, b.src.y, b.src.w - x, b.src.h};
	l.dst = {b.dst.x, b.dst.y, l.src.w, b.src.h};
	m.dst = {b.dst.x + x, b.dst.y, b.dst.w - b.src.w, b.src.h};
	r.dst = {b.dst.x + b.dst.w - r.src.w, b.dst.y, r.src.w, b.src.h};
	blits.push_back(l);
	blits.push_back(m);
	blits.push_back(r);
}

static void appendSixSplit(BlitRegionList &blits, const BlitData &b, float x) {
	const size_t first = blits.count;
	appendThreeSplit(blits, b, x);
	auto extend = b.dst.h - b.src.h;
	if (extend <= 0)
		return;

	blits.push_back(extendDown(blits.items[first], extend));
	blits.push_back(extendDown(blits.items[first + 1], extend));
	blits.push_back(extendDown(blits.items[first + 2], extend));
}

int TextWindowController::ownInit() {
	return 0;
}

int TextWindowController::ownDeinit() {
	return 0;
}

void TextWindowController::appendBottomRegion(BlitRegionList &blits, const RenderRect &window) {
	auto &main = mainRegionDimensions;
	BlitData bottom;
	bottom.src = main;
	bottom.dst = {window.x - mainRegionPadding.left, getTopOfBottom(window), window.w + mainRegionPadding.left + mainRegionPadding.right, main.h};
	appendThreeSplit(blits, bottom, mainRegionExtensionCol);
}

void TextWindowController::appendTopRegion(BlitRegionList &blits, const RenderRect &window) {
	if (dlgCtrl.dialogueName.empty())
		appendNoNameRegion(blits, window);
	else
		appendNameRegion(blits, window);
}

void TextWindowController::appendNoNameRegion(BlitRegionList &blits, const RenderRect &window) {
	BlitData top;
	top.src = noNameRegionDimensions;
	top.dst = {
	    window.x - mainRegionPadding.left,
	    window.y - mainRegionPadding.top - top.src.h,
	    window.w + mainRegionPadding.left + mainRegionPadding.right,
	    getTopOfBottom(window) - (window.y - mainRegionPadding.top - top.src.h)};
	appendSixSplit(blits, top, noNameRegionExtensionCol);
}

// Get dst size for box including namebox padding
RenderRect TextWindowController::getNameBoxRegion(const RenderRect &window) {
	auto &reg    = nameRegionDimensions;
	auto namebox = dlgCtrl.nameRenderState.bounds;
	namebox.w += nameBoxPadding.left + nameBoxPadding.right;
	namebox.h += nameBoxPadding.top + nameBoxPadding.bottom;
	// TODO : Add padding
	RenderRect box{reg.x, reg.y, nameBoxDividerCol, nameBoxExtensionRow};
	if (namebox.w == 0 || namebox.h == 0) {
		namebox = box;
	}
	if (namebox.w < box.w)
		namebox.w = box.w;
	if (namebox.h < box.h)
		namebox.h = box.h;
	namebox.x = window.x - mainRegionPadding.left;
	namebox.y = window.y - mainRegionPadding.top - reg.h - namebox.h + nameBoxExtensionRow;

	return namebox;
}

RenderRect TextWindowController::getPrintableNameBoxRegion(const RenderRect &window) {
	RenderRect fullSize = getNameBoxRegion(window);
	fullSize.x += nameBoxPadding.left;
	fullSize.y += nameBoxPadding.top;
	fullSize.w -= nameBoxPadding.left + nameBoxPadding.right;
	fullSize.h -= nameBoxPadding.top + nameBoxPadding.bottom;
	return fullSize;
}

RenderRect TextWindowController::getPrintableNameBoxRegion() {
	auto window = getExtendedWindow();
	return getPrintableNameBoxRegion(window);
}

void TextWindowController::appendNameRegion(BlitRegionList &blits, const RenderRect &window) {
	auto &reg = nameRegionDimensions;

	// These three are correct no matter what the padding; they represent the source areas in the texture map
	//  _________
	// |   box        |
	// |_________|_________
	// |   left       |    right    |
	// |_________|_________|
	// |     rest of textbox        |
	//
	RenderRect box{reg.x, reg.y, nameBoxDividerCol, nameBoxExtensionRow};
	RenderRect left{reg.x, reg.y + nameBoxExtensionRow, nameBoxDividerCol, reg.h - nameBoxExtensionRow};
	RenderRect right{reg.x + nameBoxDividerCol, reg.y + nameBoxExtensionRow, reg.w - nameBoxDividerCol, reg.h - nameBoxExtensionRow};

	// This wants to know the entire region (with padding included) so it can render the area large enough to accommodate everything
	auto namebox = getNameBoxRegion(window);

	auto topOfExtendedBottom        = window.y - mainRegionPadding.top;
	auto topOfNameRegionWithoutBox  = topOfExtendedBottom - left.h;
	auto topOfNameBox               = topOfNameRegionWithoutBox - namebox.h;
	auto nameRegionWithoutBoxHeight = getTopOfBottom(window) - topOfNameRegionWithoutBox;

	BlitData nameBoxBD;
	nameBoxBD.src = box;
	nameBoxBD.dst = RenderRect{window.x - mainRegionPadding.left, topOfNameBox, namebox.w, namebox.h};

	BlitData leftBD;
	leftBD.src = left;
	leftBD.dst = RenderRect{window.x - mainRegionPadding.left, topOfNameRegionWithoutBox, namebox.w, nameRegionWithoutBoxHeight};

	BlitData rightBD;
	rightBD.src = right;
	rightBD.dst = RenderRect{leftBD.dst.x + namebox.w, topOfNameRegionWithoutBox, window.w + mainRegionPadding.left + mainRegionPadding.right - namebox.w, nameRegionWithoutBoxHeight};

	appendSixSplit(blits, nameBoxBD, nameBoxExtensionCol);
	appendSixSplit(blits, leftBD, nameBoxExtensionCol);
	appendSixSplit(blits, rightBD, nameRegionExtensionCol - nameBoxDividerCol); // - dividerCol because this param is relative to the src rect
}

BlitRegionList TextWindowController::getRegions() {
	BlitRegionList blits;
	auto window = getExtendedWindow();
	appendTopRegion(blits, window);
	appendBottomRegion(blits, window);
	return blits;
}

RenderRect TextWindowController::getExtendedWindow() {
	return getExtendedWindow(originalWindowSize);
}

RenderRect TextWindowController::getExtendedWindow(RenderRect window) {
	window.y -= extension;
	window.h += extension;
	return window;
}

float TextWindowController::getRequiredAdditionalHeight(const RenderRect &window) {
	auto occupiedSpace   = dlgCtrl.dialogueRenderState.bounds;
	float occupiedBottom = occupiedSpace.y + occupiedSpace.h;
	float windowBottom   = window.y + window.h;
	if (occupiedBottom <= windowBottom)
		return 0;
	return occupiedBottom - windowBottom;
}

void TextWindowController::updateTextboxExtension(bool smoothly) {
	dlgCtrl.getRenderingBounds(dlgCtrl.dialogueRenderState, true);
	auto goalExtension = getRequiredAdditionalHeight(originalWindowSize);
	if (previousGoalExtension == goalExtension)
		return;
	float duration = smoothly && dlgCtrl.dialogueRenderState.segmentIndex >= 0 && !dlgCtrl.isCurrentDialogueSegmentRendered() ? 200 : 0; // make adjustable by script?
	dynamicProperties.addGlobalProperty(true, GLOBAL_PROPERTY_TEXTBOX_EXTENSION, goalExtension, duration, MOTION_EQUATION_LINEAR, true);
	previousGoalExtension = goalExtension;
}
