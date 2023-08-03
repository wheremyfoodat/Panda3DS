#pragma once
#include <array>

namespace Math {
	// Abstraction for GLSL vectors
	template <typename T, int size>
	class Vector {
		// A GLSL vector can only have 2, 3 or 4 elements
		static_assert(size == 2 || size == 3 || size == 4);
		T m_storage[size];

	  public:
		T& r() { return m_storage[0]; }
		T& g() { return m_storage[1]; }
		T& b() {
			static_assert(size >= 3, "Out of bounds OpenGL::Vector access");
			return m_storage[2];
		}
		T& a() {
			static_assert(size >= 4, "Out of bounds OpenGL::Vector access");
			return m_storage[3];
		}

		T& x() { return r(); }
		T& y() { return g(); }
		T& z() { return b(); }
		T& w() { return a(); }
		T& operator[](size_t index) { return m_storage[index]; }
		const T& operator[](size_t index) const { return m_storage[index]; }

		T& u() { return r(); }
		T& v() { return g(); }

		T& s() { return r(); }
		T& t() { return g(); }
		T& p() { return b(); }
		T& q() { return a(); }

		Vector(std::array<T, size> list) { std::copy(list.begin(), list.end(), &m_storage[0]); }

		Vector() {}
	};

	using vec2 = Vector<float, 2>;
	using vec3 = Vector<float, 3>;
	using vec4 = Vector<float, 4>;

	using dvec2 = Vector<double, 2>;
	using dvec3 = Vector<double, 3>;
	using dvec4 = Vector<double, 4>;

	using ivec2 = Vector<int, 2>;
	using ivec3 = Vector<int, 3>;
	using ivec4 = Vector<int, 4>;

	using uvec2 = Vector<unsigned int, 2>;
	using uvec3 = Vector<unsigned int, 3>;
	using uvec4 = Vector<unsigned int, 4>;

	// A 2D rectangle, meant to be used for stuff like scissor rects or viewport rects
	// We're never supporting 3D rectangles, because rectangles were never meant to be 3D in the first place
	template <typename T>
	struct Rectangle {
		Vector<T, 2> start;
		Vector<T, 2> end;

		Rectangle() : start({0}), end({0}) {}
		Rectangle(T x0, T y0, T x1, T y1) : start({x0, y0}), end({x1, y1}) {}

		T getWidth() const {
			return std::abs(end.x() - start.x());
		}

		T getHeight() const {
			return std::abs(end.y() - start.y());
		}
	};

	using Rect = Rectangle<unsigned int>;
}
