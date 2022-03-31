#ifndef SIGNALSMITH_SIG_PLOT_H
#define SIGNALSMITH_SIG_PLOT_H

#include <fstream>
#include <memory>
#include <functional>
#include <vector>
#include <array>

namespace signalsmith { namespace plot {

/// Estimates string width encoded with UTF-8
static double estimateUtf8Width(const char *utf8Str);

class PlotStyle {
public:
	std::vector<std::string> colours = {"#0073E6", "#CC0000", "#00B300", "#806600", "#E69900", "#CC00CC"};
	std::vector<std::vector<double>> dashes = {{}, {1.2, 1.2}, {2.8, 1.6}, {5, 4}, {4, 1, 1, 1, 1, 1}, {10, 3}, {4, 2, 1, 2}};
	std::string strokeClass(int styleIndex) const {
		if (styleIndex < 0) return "";
		return "svg-plot-s" + std::to_string(styleIndex%(int)colours.size());
	}
	std::string fillClass(int styleIndex) const {
		if (styleIndex < 0) return "";
		return "svg-plot-f" + std::to_string(styleIndex%(int)colours.size());
	}
	std::string dashClass(int styleIndex) const {
		if (styleIndex < 0) return "";
		return "svg-plot-d" + std::to_string(styleIndex%(int)dashes.size());
	}
	double padding = 10;
	double labelSize = 12;
	double valueSize = 10;
	double lineWidth = 1.5;
	double textAspect = 1; // If you use a different font, you might want to allocate more space for it
	double tickH = 4, tickV = 5;
	double textPadding = 5;

	std::string prefix = "", suffix = "";
	void css(std::ostream &o) {
		o << prefix;
		o << R"CSS(
			* {
				stroke-linecap: butt;
			}
			.svg-plot-bg {
				fill: none;
				stroke: none;
			}
			.svg-plot-axis {
				stroke: none;
				fill: rgba(255,255,255,0.85);
			}
			.svg-plot-line {
				stroke: blue;
				fill: none;
				stroke-width: )CSS" << lineWidth << R"CSS(px;
				stroke-linejoin: round;
			}
			.svg-plot-fill {
				stroke: none;
				opacity: 0.15;
			}
			.svg-plot-major {
				stroke: #000;
				stroke-width: 1px;
				stroke-linecap: square;
				fill: none;
			}
			.svg-plot-minor {
				stroke: rgba(0,0,0,0.3);
				stroke-width: 0.5px;
				stroke-dasharray: 0.5 1.5;
				stroke-linecap: round;
				fill: none;
			}
			.svg-plot-tick {
				stroke: #000;
				fill: none;
				stroke-width: 1px;
				stroke-linecap: butt;
			}
			.svg-plot-value, .svg-plot-label {
				font-family: Arial,sans-serif;
				fill: #000;
				stroke: rgba(255,255,255,0.7);
				stroke-width: 2px;
				paint-order: stroke fill;

				text-anchor: middle;
				dominant-baseline: central;
				alignment-baseline: baseline;
			}
			.svg-plot-label {
				font-size: )CSS" << labelSize << R"CSS(px;
			}
			.svg-plot-value {
				font-size: )CSS" << valueSize << R"CSS(px;
			}

		)CSS";
		
		for (size_t i = 0; i < colours.size(); ++i) {
			o << ".svg-plot-s" << i << "{stroke:" << colours[i] << "}\n";
			o << ".svg-plot-f" << i << "{fill:" << colours[i] << "}\n";
		}
		for (size_t i = 0; i < dashes.size(); ++i) {
			auto &d = dashes[i];
			if (d.size() == 0) {
				o << ".svg-plot-d" << i << "{stroke-width:" << (0.9*lineWidth) << "px}\n";
			} else {
				o << ".svg-plot-d" << i << "{stroke-dasharray:";
				for (auto &v : d) o << " " << (v*lineWidth);
				o << "}\n";
			}
		}
		o << suffix;
	}
};

class SvgDrawable {
	std::vector<std::unique_ptr<SvgDrawable>> children;
	bool hasLayout = false;
protected:
	struct Bounds {
		double left = 0, right = 0, top = 0, bottom = 0;
		bool set = false;
		Bounds() {}
		Bounds(double left, double right, double top, double bottom) : left(left), right(right), top(top), bottom(bottom), set(true) {}
		double width() const {
			return right - left;
		}
		double height() const {
			return bottom - top;
		}
	};
	Bounds bounds;

	virtual void layout(const PlotStyle &style) {
		if (hasLayout) {
			assert(false); // shouldn't do this more than once
		};
		hasLayout = true;
		for (auto &c : children) {
			c->layout(style);
			if (bounds.set) {
				if (c->bounds.set) {
					bounds.left = std::min(bounds.left, c->bounds.left);
					bounds.top = std::min(bounds.top, c->bounds.top);
					bounds.right = std::max(bounds.right, c->bounds.right);
					bounds.bottom = std::max(bounds.bottom, c->bounds.bottom);
				}
			} else {
				bounds = c->bounds;
			}
		}
	};
	void layoutIfNeeded(const PlotStyle &style) {
		if (!hasLayout) this->layout(style);
	}
public:
	SvgDrawable() {}
	virtual ~SvgDrawable() {}

	void addChild(SvgDrawable *child) {
		children.emplace_back(child);
	}

	/// Not copyable/assignable because that's usually a mistake
	SvgDrawable(const SvgDrawable &other) = delete;
	SvgDrawable & operator =(const SvgDrawable &other) = delete;

	/// The base implementation iterates over children in latest-first
	virtual void writeData(std::ostream &o, const PlotStyle &style) {
		// Write in reverse order
		for (int i = children.size() - 1; i >= 0; --i) {
			children[i]->writeData(o, style);
		}
	}
	/// There are two layers: the data layer and the label layer.
	virtual void writeLabel(std::ostream &o, const PlotStyle &style) {
		// Write in reverse order
		for (int i = children.size() - 1; i >= 0; --i) {
			children[i]->writeLabel(o, style);
		}
	}

	static void escape(std::ostream &o, const char *str) {
		while (*str) {
			if (*str == '<') {
				o << "&lt;";
			} else if (*str == '&') {
				o << "&amp;";
			} else if (*str == '"') {
				o << "&quot;";
			} else {
				o << (*str);
			}
			++str;
		}
	}
	static void escape(std::ostream &o, const std::string &str) {
		escape(o, str.c_str());
	}
};

class Axis1D {
	std::function<double(double)> unitMap;
	double autoMin, autoMax;
	bool hasAutoValue = false;
	bool autoScale, autoLabel;
public:
	double drawLow, drawHigh;
	double drawMin() const {
		return std::min(drawLow, drawHigh);
	}
	double drawMax() const {
		return std::max(drawLow, drawHigh);
	}
	double drawSize() const {
		return std::abs(drawHigh - drawLow);
	}

	void autoValue(double v) {
		if (!autoScale) return;
		if (!hasAutoValue) {
			autoMin = autoMax = v;
			hasAutoValue = true;
		} else {
			autoMin = std::min(autoMin, v);
			autoMax = std::max(autoMax, v);
		}
	}
	void autoSetup() {
		if (hasAutoValue) {
			if (autoScale) linear(autoMin, autoMax);
			if (autoLabel) minor(autoMin, autoMax);
		}
	}

	Axis1D(double drawLow, double drawHigh) : drawLow(drawLow), drawHigh(drawHigh) {
		linear(0, 1);
		autoScale = true;
		autoLabel = true;
	}
	
	Axis1D & linear(double low, double high) {
		autoScale = false;
		unitMap = [=](double v) {
			return (v - low)/(high - low);
		};
		return *this;
	}
	
	double map(double v) {
		double unit = unitMap(v);
		return drawLow + unit*(drawHigh - drawLow);
	}

	struct Tick {
		double value;
		std::string name;
		int strength = 0;
		
		Tick(double value, std::string name) : value(value), name(name) {}

		template<typename T>
		Tick(T value) : value(double(value)), name(std::to_string(value)) {}
	};
	std::vector<Tick> ticks;
	
	Axis1D &major() {
		autoLabel = false;
		return *this;
	}
	template<class ...Args>
	Axis1D &major(Tick tick, Args ...args) {
		autoValue(tick.value);
		tick.strength = 2;
		ticks.push_back(tick);
		return major(args...);
	}
	template<typename T>
	Axis1D & majorRange(T start, T end, T step, bool useLabels=true) {
		step = std::abs(step);
		if (start <= end) {
			for (T t = start; t <= end; t += step) {
				useLabels ? major(t) : major({double(t), ""});
			}
		} else {
			for (T t = end; t >= start; t -= step) {
				useLabels ? major(t) : major({double(t), ""});
			}
		}
		return *this;
	}
	Axis1D &minor() {
		autoLabel = false;
		return *this;
	}
	template<class ...Args>
	Axis1D &minor(Tick tick, Args ...args) {
		autoValue(tick.value);
		tick.strength = 1;
		ticks.push_back(tick);
		return minor(args...);
	}
	template<typename T>
	Axis1D & minorRange(T start, T end, T step, bool useLabels=true) {
		step = std::abs(step);
		if (start <= end) {
			for (T t = start; t <= end; t += step) {
				useLabels ? minor(t) : minor({double(t), ""});
			}
		} else {
			for (T t = end; t >= start; t -= step) {
				useLabels ? minor(t) : minor({double(t), ""});
			}
		}
		return *this;
	}
	Axis1D &tick() {
		autoLabel = false;
		return *this;
	}
	template<class ...Args>
	Axis1D &tick(Tick t, Args ...args) {
		autoValue(t.value);
		t.strength = 0;
		ticks.push_back(t);
		return tick(args...);
	}
	template<typename T>
	Axis1D & tickRange(T start, T end, T step, bool useLabels=true) {
		step = std::abs(step);
		if (start <= end) {
			for (T t = start; t <= end; t += step) {
				useLabels ? tick(t) : tick({double(t), ""});
			}
		} else {
			for (T t = end; t >= start; t -= step) {
				useLabels ? tick(t) : tick({double(t), ""});
			}
		}
		return *this;
	}
};

struct Point {
	double x, y;
};

template<bool isValue=false>
class TextLabel : public SvgDrawable {
	double textWidth = 0;
	void write(std::ostream &o) {
		o << "<text class=\"";
		escape(o, cssClass);
		o << "\"";
		double tx = drawAt.x, ty = drawAt.y;
		if (alignment > 0.5) {
			o << " style=\"text-anchor:start\"";
			tx += textWidth*(alignment - 1);
		} else if (alignment < -0.5) {
			o << " style=\"text-anchor:end\"";
			tx += textWidth*(alignment + 1);
		} else {
			tx += textWidth*alignment;
		}
		o << " x=\"" << tx << "\" y=\"" << ty << "\"";
		o << ">";
		escape(o, text);
		o << "</text>";
	}
protected:
	Point drawAt;
	double alignment = 0; // 0=centre, 1=left, -1=right
	std::string text, cssClass;
public:
	TextLabel(Point at, double alignment, std::string text, std::string cssClass="svg-plot-label") : drawAt(at), alignment(alignment), text(text), cssClass(cssClass) {}
	
	void layout(const PlotStyle &style) override {
		double x = drawAt.x, y = drawAt.y;
		double fontSize = isValue ? style.valueSize : style.labelSize;
		this->bounds = {x, x, y - fontSize*0.5, y + fontSize*0.5};

		// Assume all text/labels are UTF-8
		textWidth = estimateUtf8Width(text.c_str())*fontSize*style.textAspect;

		this->bounds.right += textWidth*(alignment + 1)*0.5;
		this->bounds.left += textWidth*(alignment - 1)*0.5;
	}

	void writeLabel(std::ostream &o, const PlotStyle &) override {
		write(o);
	}
};

class Line2D;
class Fill2D;

class Axes2D : public SvgDrawable {
	int styleIndex = 0;
public:
	Axis1D x, y;

	Axes2D(Axis1D x, Axis1D y) : x(x), y(y) {}
	
	void writeData(std::ostream &o, const PlotStyle &style) override {
		double padding = style.lineWidth*0.5;
		long clipId = rand();
		
		o << "<rect class=\"svg-plot-axis\" x=\"" << x.drawMin() << "\" y=\"" << y.drawMin() << "\" width=\"" << x.drawSize() << "\" height=\"" << y.drawSize() << "\"/>";
		for (auto &t : x.ticks) {
			if (t.strength) {
				double screenX = x.map(t.value);
				bool isLeftBorder = std::abs(screenX - x.drawMin()) < 0.01; // 1% of a pixel is close enough
				double extraTop = (t.strength == 2 && isLeftBorder && t.name.size()) ? style.tickH : 0; // Extend using horizontal tick even though it's vertical
				o << "<line class=\"svg-plot-" << (t.strength == 2 ? "major" : "minor") << "\" y1=\"" << (y.drawMin() - extraTop) << "\" y2=\"" << y.drawMax() << "\" x1=\"" << screenX << "\" x2=\"" << screenX << "\"/>";
			}
		}
		for (auto &t : y.ticks) {
			if (t.strength) {
				double screenY = y.map(t.value);
				o << "<line class=\"svg-plot-" << (t.strength == 2 ? "major" : "minor") << "\" x1=\"" << x.drawMin() << "\" x2=\"" << x.drawMax() << "\" y1=\"" << screenY << "\" y2=\"" << screenY << "\"/>";
			}
		}

		o << "<clipPath id=\"clip" << clipId << "\"><rect x=\"" << (x.drawMin() - padding) << "\" y=\"" << (y.drawMin() - padding) << "\" width=\"" << (x.drawSize() + padding*2) << "\" height=\"" << (y.drawSize() + padding*2) << "\" /></clipPath>";
		o << "<g clip-path=\"url(#clip" << clipId << "\">";
		SvgDrawable::writeData(o, style);
		o << "</g>";
	}

	void writeLabel(std::ostream &o, const PlotStyle &style) override {
		o << "<g>";
		SvgDrawable::writeLabel(o, style);

		for (auto &t : x.ticks) {
			double screenX = x.map(t.value);
			if (t.name.size()) {
				o << "<line class=\"svg-plot-tick\" y1=\"" << y.drawMax() << "\" y2=\"" << (y.drawMax() + style.valueSize*0.5) << "\" x1=\"" << screenX << "\" x2=\"" << screenX << "\"/>";
			}
		}
		for (auto &t : y.ticks) {
			if (t.name.size()) {
				double screenY = y.map(t.value);
				o << "<line class=\"svg-plot-tick\" x1=\"" << (x.drawMin() - style.tickH) << "\" x2=\"" << x.drawMin() << "\" y1=\"" << screenY << "\" y2=\"" << screenY << "\"/>";
			}
		}
		o << "</g>";
	}

	void layout(const PlotStyle &style) override {
		// Auto-scale axes if needed
		x.autoSetup();
		y.autoSetup();

		// Add labels for axes
		double screenY = y.drawMax() + style.tickV + style.valueSize*0.5 + style.textPadding;
		for (auto &t : x.ticks) {
			if (t.name.size()) {
				double screenX = x.map(t.value);
				auto *label = new TextLabel<true>({screenX, screenY}, 0, t.name, "svg-plot-value");
				this->addChild(label);
			}
		}
		double screenX = x.drawMin() - style.tickH - style.textPadding;
		for (auto &t : y.ticks) {
			if (t.name.size()) {
				double screenY = y.map(t.value);
				auto *label = new TextLabel<true>({screenX, screenY}, -1, t.name, "svg-plot-value");
				this->addChild(label);
			}
		}

		this->bounds = {
			x.drawMin() - style.tickH,
			x.drawMax() + style.tickH,
			y.drawMin() - style.tickV,
			y.drawMax() + style.tickV
		};

		SvgDrawable::layout(style);
	};
	
	Line2D & line(int style);
	Line2D & line() {
		return line(styleIndex++);
	}

	Fill2D & fill(int style);
	Fill2D & fill() {
		return fill(styleIndex++);
	}
};

class Line2D : public SvgDrawable {
	Axes2D &axes;
	std::vector<Point> points;
	int styleIndex = 0;
public:
	Line2D(Axes2D &axes, int styleIndex) : axes(axes), styleIndex(styleIndex) {}
	
	Line2D & add(double x, double y) {
		points.push_back({x, y});
		axes.x.autoValue(x);
		axes.y.autoValue(y);
		return *this;
	}
	
	class LineLabel : public TextLabel<false> {
		Axis1D &axisX, &axisY;
		Point at;
		std::string name;
		// direction: 0=right, 1=up, 2=left, 3=down
		double direction, distance;
		Point drawLineFrom{0, 0}, drawLineTo{0, 0};
		int styleIndex;
	public:
		LineLabel(Axis1D &axisX, Axis1D &axisY, Point at, std::string name, double direction, double distance, int styleIndex) : TextLabel({0, 0}, 0, name), axisX(axisX), axisY(axisY), at(at), name(name), direction(direction), distance(distance), styleIndex(styleIndex) {}
		
		void layout(const PlotStyle &style) override {
			double angle = direction*-0.5*3.14159265358979;
			double ax = std::cos(angle), ay = std::sin(angle);

			double sx = axisX.map(at.x), sy = axisY.map(at.y);
			double px = sx + distance*ax, py = sy + distance*ay;
			double tx = px, ty = py;
			double fontSize = style.labelSize;
			ty -= fontSize*0.1; // Just a vertical alignment tweak

			double space = fontSize*0.25;
			double verticalWiggle = fontSize*0.3;
			if (ax < -0.7) {
				this->alignment = -1;
				tx -= space;
				ty += ay*verticalWiggle;
			} else if (ax > 0.7) {
				this->alignment = 1;
				tx += space;
				ty += ay*verticalWiggle;
			} else if (ay > 0) {
				ty += fontSize*0.8;
				tx += ax*fontSize;
				this->alignment = tx;
			} else {
				ty -= fontSize*0.8;
				tx += ax*fontSize;
				this->alignment = tx;
			}
			
			double lineDistance = distance - space;
			drawLineFrom = drawLineTo = {px, py};
			if (lineDistance > space) {
				drawLineTo = {sx + ax*space, sy + ay*space};
			}

			this->drawAt = {tx, ty};
			this->cssClass = "svg-plot-label " + style.fillClass(styleIndex);

			TextLabel<false>::layout(style);
		}
		
		void writeLabel(std::ostream &o, const PlotStyle &style) override {
			if (drawLineTo.x != drawLineFrom.x || drawLineTo.y != drawLineFrom.y) {
				o << "<line class=\"svg-plot-tick " << style.strokeClass(styleIndex) << "\" x1=\"" << drawLineFrom.x << "\" x2=\"" << drawLineTo.x << "\" y1=\"" << drawLineFrom.y << "\" y2=\"" << drawLineTo.y << "\"/>";
			}
			TextLabel<false>::writeLabel(o, style);
		}
	};

	Line2D & label(double valueX, double valueY, std::string name, double direction=0, double distance=0) {
		axes.x.autoValue(valueX);
		axes.y.autoValue(valueY);
		axes.addChild(new LineLabel(axes.x, axes.y, {valueX, valueY}, name, direction, distance, styleIndex));
		return *this;
	}

	Line2D & label(std::string name, double direction=0, double distance=0) {
		Point latest = points.back();
		return label(latest.x, latest.y, name, direction, distance);
	}

	Line2D & label(double xIsh, std::string name, double direction=0, double distance=0) {
		size_t closest = 0;
		double closestError = -1;
		for (size_t i = 0; i < points.size(); ++i) {
			if (closestError < 0 || closestError > std::abs(points[i].x - xIsh)) {
				closest = i;
				closestError = std::abs(points[i].x - xIsh);
			}
		}
		Point latest = points[closest];
		return label(latest.x, latest.y, name, direction, distance);
	}
	
	void writeData(std::ostream &o, const PlotStyle &style) override {
		o << "<path class=\"svg-plot-line " << style.strokeClass(styleIndex) << " " << style.dashClass(styleIndex) << "\" d=\"M";
		for (auto &p : points) {
			o << " " << axes.x.map(p.x) << " " << axes.y.map(p.y);
		}
		o << "\" />";

		SvgDrawable::writeData(o, style);
	}
};
Line2D & Axes2D::line(int style) {
	Line2D *line = new Line2D(*this, style);
	this->addChild(line);
	return *line;
}

class Fill2D : public SvgDrawable {
	Axes2D &axes;
	struct Point {
		double x, y;
	};
	std::vector<Point> points;
	int styleIndex = 0;
public:
	Fill2D(Axes2D &axes, int styleIndex) : axes(axes), styleIndex(styleIndex) {}
	
	Fill2D & add(double x, double y) {
		points.push_back({x, y});
		axes.x.autoValue(x);
		axes.y.autoValue(y);
		return *this;
	}
	
	void writeData(std::ostream &o, const PlotStyle &style) override {
		o << "<path class=\"svg-plot-fill " << style.fillClass(styleIndex) << "\" d=\"M";
		for (auto &p : points) {
			o << " " << axes.x.map(p.x) << " " << axes.y.map(p.y);
		}
		o << "\" />";

		SvgDrawable::writeData(o, style);
	}
};
Fill2D & Axes2D::fill(int style) {
	Fill2D *fill = new Fill2D(*this, style);
	this->addChild(fill);
	return *fill;
}

class Plot : public SvgDrawable {
	double width = 4.8*72*0.75;
	double height = 2.5*72*0.75;
public:
	PlotStyle style;

	Axes2D & axes() {
		Axes2D *axes = new Axes2D({0, width}, {height, 0});
		this->addChild(axes);
		return *axes;
	}
	
	template<class ...Args>
	Axis1D::Tick tick(Args ...args) {
		return Axis1D::Tick(args...);
	}
	
	void layout(const PlotStyle &style) override {
		this->bounds = {0, width, 0, height};
		SvgDrawable::layout(style);
		this->bounds.left -= style.padding;
		this->bounds.right += style.padding;
		this->bounds.top -= style.padding;
		this->bounds.bottom += style.padding;
	}

	void write(std::ostream &o) {
		this->layoutIfNeeded(style);
		o << "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
		o << "<svg class=\"svg-plot\" width=\"" << bounds.width() << "pt\" height=\"" << bounds.height() << "pt\" version=\"1.1\" viewBox=\"" << bounds.left << " " << bounds.top << " " << bounds.width() << " " << bounds.height() << "\" preserveAspectRatio=\"xMidYMid\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">";

		o << "<rect class=\"svg-plot-bg\" x=\"" << this->bounds.left << "\" y=\"" << this->bounds.top << "\" width=\"" << this->bounds.width() << "\" height=\"" << this->bounds.height() << "\"/>";
		SvgDrawable::writeData(o, style);
		SvgDrawable::writeLabel(o, style);

		o << "<style>";
		std::stringstream cssStream;
		style.css(cssStream);
		std::string css = cssStream.str();
		const char *c = css.c_str();
		// Strip tabs and newlines;
		while (*c) {
			if (*c != '\t' && *c != '\n') {
				// skip space before {
				if (*c != ' ' || *(c + 1) != '{') {
					o << (*c);
				}
			}
			// skip space after :
			if ((*c == ':' || *c == ',') && *(c + 1) == ' ') ++c;
			++c;
		}
		o << "</style><script>var q={};location.href.replace(/^[^#]*#?/,'').split('\\x26').forEach(function(p,D){D=decodeURIComponent;if(p){p=p.split('=');q[D(p.shift())]=D(p.join('='))}});var d=document,r=d.rootElement,s=d.getElementsByTagName('style')[0];if(q.style){s.textContent='@import \"'+q.style+'\";'+s.textContent}if(q.css){s.textContent+=q.css}</script></svg>";
	}

	void write(std::string svgFile) {
		std::ofstream s(svgFile);
		write(s);
	}
};

static double estimateCharWidth(int c) {
	// measured experimentally, covering basic Latin (no accents) and Greek
	if (c >= 32 && c < 127) {
		static char w[95] = {31, 36, 45, 70, 61, 95, 77, 29, 39, 39, 40, 72, 31, 39, 31, 44, 61, 54, 58, 59, 59, 58, 59, 58, 59, 59, 38, 38, 74, 100, 74, 54, 97, 69, 66, 71, 76, 64, 62, 76, 77, 41, 53, 69, 57, 89, 76, 78, 63, 80, 68, 64, 62, 75, 67, 96, 69, 64, 64, 41, 46, 41, 68, 59, 54, 57, 59, 52, 59, 56, 38, 58, 58, 29, 33, 53, 30, 87, 58, 57, 59, 59, 43, 49, 38, 58, 53, 77, 54, 53, 50, 47, 46, 47, 69};
		return w[c - 32]*0.01;
	} else if (c == 168) {
		return 0.53;
	} else if (c == 183) {
		return 0.33;
	} else if (c == 697) {
		return 0.26;
	} else if (c >= 880 && c < 884) {
		static char w[4] = {42, 31, 64, 52};
		return w[c - 880]*0.01;
	} else if (c >= 885 && c < 888) {
		static char w[3] = {40, 66, 48};
		return w[c - 885]*0.01;
	} else if (c >= 890 && c < 894) {
		static char w[4] = {33, 52, 52, 52};
		return w[c - 890]*0.01;
	} else if (c == 895) {
		return 0.33;
	} else if (c == 900) {
		return 0.52;
	} else if (c >= 913 && c < 930) {
		static char w[17] = {75, 71, 63, 73, 71, 71, 82, 82, 45, 77, 75, 94, 81, 70, 83, 85, 67};
		return w[c - 913]*0.01;
	} else if (c >= 931 && c < 938) {
		static char w[7] = {69, 65, 70, 82, 80, 85, 84};
		return w[c - 931]*0.01;
	} else if (c >= 945 && c < 970) {
		static char w[25] = {61, 58, 57, 57, 49, 50, 58, 60, 29, 57, 55, 59, 53, 51, 57, 63, 59, 50, 59, 48, 58, 72, 56, 76, 76};
		return w[c - 945]*0.01;
	} else if (c >= 975 && c < 979) {
		static char w[4] = {47, 66, 74, 66};
		return w[c - 975]*0.01;
	} else if (c >= 981 && c < 1024) {
		static char w[43] = {80, 86, 56, 79, 63, 68, 67, 57, 53, 60, 53, 75, 85, 86, 85, 69, 56, 70, 53, 69, 69, 61, 61, 75, 56, 43, 37, 59, 63, 46, 29, 79, 55, 55, 62, 63, 71, 87, 75, 75, 75, 75, 75};
		return w[c - 981]*0.01;
	} else if (c == 65291) {
		return 1;
	}
	return 0.85;
}

/// Estimates string width encoded with UTF-8
static double estimateUtf8Width(const char *utf8Str) {
	/// TODO: UTF-8 decoding!
	double total = 0;
	while (*utf8Str) {
		total += estimateCharWidth(*utf8Str);
		++utf8Str;
	}
	return total;
}


}}; // namespace

#endif // include guard
