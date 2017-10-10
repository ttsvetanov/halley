#include "ui_sizer.h"
#include "ui_widget.h"

using namespace Halley;

UISizerEntry::UISizerEntry()
{
}

UISizerEntry::UISizerEntry(UIElementPtr widget, float proportion, Vector4f border, int fillFlags)
	: widget(widget)
	, proportion(proportion)
	, border(border)
	, fillFlags(fillFlags)
{
}

float UISizerEntry::getProportion() const
{
	return proportion;
}

Vector2f UISizerEntry::getMinimumSize() const
{
	return widget ? widget->getLayoutMinimumSize() : Vector2f();
}

void UISizerEntry::placeInside(Rect4f rect, Vector2f minSize)
{
	Vector2f cellSize = rect.getSize();
	Vector2f anchoring;
	Vector2f size = minSize;
	
	if (fillFlags & UISizerAlignFlags::Top) {
		anchoring.y = 0.0f;
	}
	if (fillFlags & UISizerAlignFlags::CentreVertical) {
		anchoring.y = 0.5f;
	}
	if (fillFlags & UISizerAlignFlags::Bottom) {
		anchoring.y = 1.0f;
	}
	if (fillFlags & UISizerAlignFlags::Left) {
		anchoring.x = 0.0f;
	}
	if (fillFlags & UISizerAlignFlags::CentreHorizontal) {
		anchoring.x = 0.5f;
	}
	if (fillFlags & UISizerAlignFlags::Right) {
		anchoring.x = 1.0f;
	}
	if (fillFlags & UISizerFillFlags::FillHorizontal) {
		size.x = cellSize.x;
	}
	if (fillFlags & UISizerFillFlags::FillVertical) {
		size.y = cellSize.y;
	}

	Vector2f spareSize = cellSize - size;
	Vector2f pos = (rect.getTopLeft() + spareSize * anchoring).round();

	if (widget) {
		widget->setRect(Rect4f(pos, pos + size));
	}
}

UIElementPtr UISizerEntry::getPointer() const
{
	return widget;
}

bool UISizerEntry::isEnabled() const
{
	return !widget || widget->isActive();
}

Vector4f UISizerEntry::getBorder() const
{
	return border;
}

int UISizerEntry::getFillFlags() const
{
	return fillFlags;
}

UISizer::UISizer(UISizerType type, float gap, int nColumns)
	: type(type)
	, gap(gap)
	, nColumns(nColumns)
{
}

Vector2f UISizer::getLayoutMinimumSize() const
{
	return computeMinimumSize(true);
}

Vector2f UISizer::computeMinimumSize(bool includeProportional) const
{
	if (type == UISizerType::Horizontal || type == UISizerType::Vertical) {
		return computeMinimumSizeBox(includeProportional);
	} else {
		return computeMinimumSizeGrid();
	}
}

void UISizer::setRect(Rect4f rect)
{
	if (type == UISizerType::Horizontal || type == UISizerType::Vertical) {
		setRectBox(rect);
	} else {
		setRectGrid(rect);
	}
}

void UISizer::add(std::shared_ptr<UIWidget> widget, float proportion, Vector4f border, int fillFlags)
{
	addElement(widget, proportion, border, fillFlags);
}

void UISizer::add(std::shared_ptr<UISizer> sizer, float proportion, Vector4f border, int fillFlags)
{
	addElement(sizer, proportion, border, fillFlags);
}

void UISizer::addSpacer(float size)
{
	entries.emplace_back(UISizerEntry({}, 0, Vector4f(type == UISizerType::Horizontal ? size : 0.0f, type == UISizerType::Vertical ? size : 0.0f, 0.0f, 0.0f), {}));
}

void UISizer::addStretchSpacer(float proportion)
{
	entries.emplace_back(UISizerEntry({}, proportion, {}, {}));
}

void UISizer::reparent(UIParent& parent)
{
	for (auto& e: entries) {
		auto widget = std::dynamic_pointer_cast<UIWidget>(e.getPointer());
		if (widget) {
			parent.addChild(widget);
		} else {
			auto sizer = std::dynamic_pointer_cast<UISizer>(e.getPointer());
			if (sizer) {
				sizer->reparent(parent);
			}
		}
	}
}

void UISizer::addElement(UIElementPtr widget, float proportion, Vector4f border, int fillFlags)
{
	entries.emplace_back(UISizerEntry(widget, proportion, border, fillFlags));
}

UISizerType UISizer::getType() const
{
	return type;
}

size_t UISizer::size() const
{
	return entries.size();
}

const UISizerEntry& UISizer::operator[](size_t n) const
{
	return entries[n];
}

UISizerEntry& UISizer::operator[](size_t n)
{
	return entries[n];
}

void UISizer::clear()
{
	for (auto& e: entries) {
		auto widget = std::dynamic_pointer_cast<UIWidget>(e.getPointer());
		if (widget) {
			widget->destroy();
		} else {
			auto sizer = std::dynamic_pointer_cast<UISizer>(e.getPointer());
			if (sizer) {
				sizer->clear();
			}
		}
	}
	entries.clear();
}

bool UISizer::isActive() const
{
	for (auto& e: entries) {
		if (e.isEnabled()) {
			return true;
		}
	}
	return false;
}

void UISizer::setColumnProportions(const std::vector<float>& values)
{
	columnProportions = values;
}

void UISizer::setEvenColumns()
{
	columnProportions.resize(nColumns);
	for (auto& c: columnProportions) {
		c = 1.0f;
	}
}

Vector2f UISizer::computeMinimumSizeBox(bool includeProportional) const
{
	float totalProportion = 0;

	for (auto& e: entries) {
		if (e.isEnabled()) {
			totalProportion += e.getProportion();
		}
	}

	int mainAxis = type == UISizerType::Horizontal ? 0 : 1;
	int otherAxis = 1 - mainAxis;
	
	float main = 0;
	float biggestProportional = 0;
	float other = 0;
	
	bool first = true;
	for (auto& e: entries) {
		if (!e.isEnabled()) {
			continue;
		}

		Vector2f sz = e.getMinimumSize();
		auto border = e.getBorder();
		if (!first) {
			border[mainAxis] += gap;
		}
		first = false;
		
		other = std::max(other, sz[otherAxis] + border[otherAxis] + border[otherAxis + 2]);

		float p = e.getProportion();
		if (p > 0.0001f) {
			float s = sz[mainAxis] / p;
			biggestProportional = std::max(biggestProportional, s);
		} else {
			main += sz[mainAxis];
		}

		main += border[mainAxis] + border[mainAxis + 2];
	}
	if (includeProportional) {
		main += biggestProportional * totalProportion;
	}

	Vector2f result;
	result[mainAxis] = std::max(0.0f, main);
	result[otherAxis] = std::max(0.0f, other);

	return result;
}

void UISizer::setRectBox(Rect4f rect)
{
	Vector2f pos = rect.getTopLeft();

	float totalProportion = 0;
	for (auto& e: entries) {
		if (e.isEnabled()) {
			totalProportion += e.getProportion();
		}
	}

	int mainAxis = type == UISizerType::Horizontal ? 0 : 1;
	int otherAxis = 1 - mainAxis;

	Vector2f sizerMinSize = computeMinimumSizeBox(false);
	float spare = (rect.getSize() - sizerMinSize)[mainAxis];
	
	bool first = true;
	for (auto& e: entries) {
		if (!e.isEnabled()) {
			continue;
		}

		float p = e.getProportion();
		auto border = e.getBorder();
		if (!first) {
			border[mainAxis] += gap;
		}
		first = false;

		Vector2f minSize = e.getMinimumSize();
		Vector2f cellSize = minSize;
		if (p > 0.0001f) {
			float propSize = std::floor(spare * p / totalProportion);
			spare -= propSize;
			totalProportion -= p;
			cellSize[mainAxis] = propSize;
		}
		cellSize[otherAxis] = rect.getSize()[otherAxis] - border[otherAxis] - border[otherAxis + 2];

		Vector2f curPos = pos + Vector2f(border.x, border.y);
		e.placeInside(Rect4f(curPos, curPos + cellSize), minSize);

		pos[mainAxis] += cellSize[mainAxis] + border[mainAxis] + border[mainAxis + 2];
	}
}

void UISizer::computeGridSizes(std::vector<float>& colSize, std::vector<float>& rowSize) const
{
	Expects(nColumns > 0);
	int nRows = std::max(1, int((entries.size() + nColumns - 1) / nColumns));

	colSize.resize(nColumns, 0.0f);
	rowSize.resize(nRows, 0.0f);
	
	// Update the minimum requirement for each cell
	{
		int i = 0;
		for (auto& e: entries) {
			int x = i % nColumns;
			int y = i / nColumns;
			++i;

			if (!e.isEnabled()) {
				continue;
			}

			Vector2f sz = e.getMinimumSize();
			auto border = e.getBorder();
			colSize[x] = std::max(colSize[x], sz.x + border.x + border.z);
			rowSize[y] = std::max(rowSize[y], sz.y + border.y + border.w);
		}
	}

	// From here on: ensure that columns respect the proportion between them
	float minMult = 0;

	// First pass: gather data
	for (int i = 0; i < int(colSize.size()); ++i) {
		float p = getColumnProportion(i);
		if (p > 0) {
			minMult = std::max(minMult, colSize[i] / p);
		}
	}

	// Second pass: apply
	for (int i = 0; i < int(colSize.size()); ++i) {
		float p = getColumnProportion(i);
		if (p > 0) {
			colSize[i] = std::max(colSize[i], p * minMult);
		}
	}
}

Vector2f UISizer::computeMinimumSizeGrid() const
{
	std::vector<float> colSize;
	std::vector<float> rowSize;
	computeGridSizes(colSize, rowSize);

	Vector2f result((colSize.size() - 1) * gap, (rowSize.size() - 1) * gap);
	for (auto c: colSize) {
		result.x += c;
	}
	for (auto c: rowSize) {
		result.y += c;
	}
	return result;
}

void UISizer::setRectGrid(Rect4f rect)
{
	Expects(nColumns > 0);
	int nRows = int((entries.size() + nColumns - 1) / nColumns);
	
	Vector2f startPos = rect.getTopLeft();

	std::vector<float> cols(nColumns);
	std::vector<float> rows(nRows);
	std::vector<float> colSize;
	std::vector<float> rowSize;
	computeGridSizes(colSize, rowSize);

	// Add up min width
	float minWidth = (colSize.size() - 1) * gap;
	for (auto c: colSize) {
		minWidth += c;
	}
	float spareWidth = std::max(0.0f, rect.getWidth() - minWidth);

	// Respect proportion
	float totalProportion = 0;
	for (auto& p: columnProportions) {
		totalProportion += p;
	}
	if (totalProportion > 0) {
		// Distribute spare
		for (int i = 0; i < nColumns; ++i) {
			colSize[i] += spareWidth * getColumnProportion(i) / totalProportion;
		}
	}

	{
		float x = 0;
		for (int i = 0; i < nColumns; ++i) {
			cols[i] = x;
			x += colSize[i] + gap;
		}
		float y = 0;
		for (int i = 0; i < nRows; ++i) {
			rows[i] = y;
			y += rowSize[i] + gap;
		}
	}

	int i = 0;
	for (auto& e: entries) {
		int x = i % nColumns;
		int y = i / nColumns;
		++i;

		if (!e.isEnabled()) {
			continue;
		}

		Vector2f cellSize(colSize[x], rowSize[y]);
		Vector2f sz = e.getMinimumSize();
		auto border = e.getBorder();
		Vector2f curPos = Vector2f(cols[x], rows[y]) + startPos + Vector2f(border.x, border.y);
		e.placeInside(Rect4f(curPos, curPos + cellSize), sz);
	}
}

float UISizer::getColumnProportion(int col) const
{
	if (col >= 0 && col < int(columnProportions.size())) {
		return columnProportions[col];
	}
	return 0.0f;
}
