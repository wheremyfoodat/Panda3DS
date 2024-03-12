#include <nihstro/inline_assembly.h>

#include <PICA/dynapica/shader_rec.hpp>
#include <PICA/shader.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <memory>

using namespace Floats;
static const nihstro::SourceRegister input0 = nihstro::SourceRegister::MakeInput(0);
static const nihstro::SourceRegister input1 = nihstro::SourceRegister::MakeInput(1);
static const nihstro::DestRegister output0 = nihstro::DestRegister::MakeOutput(0);

static std::unique_ptr<PICAShader> assembleVertexShader(std::initializer_list<nihstro::InlineAsm> code) {
	const auto shaderBinary = nihstro::InlineAsm::CompileToRawBinary(code);
	auto newShader = std::make_unique<PICAShader>(ShaderType::Vertex);
	newShader->reset();

	for (const nihstro::Instruction& instruction : shaderBinary.program) {
		newShader->uploadWord(instruction.hex);
	}
	for (const nihstro::SwizzlePattern& swizzle : shaderBinary.swizzle_table) {
		newShader->uploadDescriptor(swizzle.hex);
	}
	newShader->finalize();
	return newShader;
}

class ShaderInterpreterTest final {
  private:
	std::unique_ptr<PICAShader> shader = {};

  public:
	explicit ShaderInterpreterTest(std::initializer_list<nihstro::InlineAsm> code) : shader(assembleVertexShader(code)) {}

	// Multiple inputs, singular scalar output
	float runScalar(std::initializer_list<float> inputs) {
		usize inputIndex = 0;
		for (const float& input : inputs) {
			const std::array<Floats::f24, 4> input_vec = std::array<Floats::f24, 4>{f24::fromFloat32(input), f24::zero(), f24::zero(), f24::zero()};
			shader->inputs[inputIndex++] = input_vec;
		}
		shader->run();
		return shader->outputs[0][0];
	}

	static std::unique_ptr<ShaderInterpreterTest> assembleTest(std::initializer_list<nihstro::InlineAsm> code) {
		return std::make_unique<ShaderInterpreterTest>(code);
	}
};

#if defined(PANDA3DS_SHADER_JIT_SUPPORTED)
class ShaderJITTest final {
  private:
	std::unique_ptr<PICAShader> shader = {};
	ShaderJIT shaderJit = {};

  public:
	explicit ShaderJITTest(std::initializer_list<nihstro::InlineAsm> code) : shader(assembleVertexShader(code)) { shaderJit.prepare(*shader.get()); }

	// Multiple inputs, singular scalar output
	float runScalar(std::initializer_list<float> inputs) {
		usize inputIndex = 0;
		for (const float& input : inputs) {
			const std::array<Floats::f24, 4> input_vec = std::array<Floats::f24, 4>{f24::fromFloat32(input), f24::zero(), f24::zero(), f24::zero()};
			shader->inputs[inputIndex++] = input_vec;
		}
		shaderJit.run(*shader.get());
		return shader->outputs[0][0];
	}

	static std::unique_ptr<ShaderJITTest> assembleTest(std::initializer_list<nihstro::InlineAsm> code) {
		return std::make_unique<ShaderJITTest>(code);
	}
};
#define SHADER_TEST_CASE(NAME, TAG) TEMPLATE_TEST_CASE(NAME, TAG, ShaderInterpreterTest, ShaderJITTest)
#else
#define SHADER_TEST_CASE(NAME, TAG) TEMPLATE_TEST_CASE(NAME, TAG, ShaderInterpreterTest)
#endif

SHADER_TEST_CASE("ADD", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::ADD, output0, input0, input1},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({+1.0f, -1.0f}) == +0.0f);
	REQUIRE(shader->runScalar({+0.0f, -0.0f}) == -0.0f);
	REQUIRE(std::isnan(shader->runScalar({+INFINITY, -INFINITY})));
	REQUIRE(std::isinf(shader->runScalar({INFINITY, +1.0f})));
	REQUIRE(std::isinf(shader->runScalar({INFINITY, -1.0f})));
}

SHADER_TEST_CASE("MUL", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::MUL, output0, input0, input1},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({+1.0f, -1.0f}) == -1.0f);
	REQUIRE(shader->runScalar({-1.0f, +1.0f}) == -1.0f);
	REQUIRE(shader->runScalar({INFINITY, 0.0f}) == 0.0f);
	REQUIRE(shader->runScalar({+INFINITY, +INFINITY}) == INFINITY);
	REQUIRE(shader->runScalar({+INFINITY, -INFINITY}) == -INFINITY);
	REQUIRE(std::isnan(shader->runScalar({NAN, 0.0f})));
}

SHADER_TEST_CASE("RCP", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::RCP, output0, input0},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({-0.0f}) == INFINITY);
	REQUIRE(shader->runScalar({0.0f}) == INFINITY);
	REQUIRE(shader->runScalar({INFINITY}) == 0.0f);
	REQUIRE(std::isnan(shader->runScalar({NAN})));

	REQUIRE(shader->runScalar({16.0f}) == Catch::Approx(0.0625f).margin(0.001f));
	REQUIRE(shader->runScalar({8.0f}) == Catch::Approx(0.125f).margin(0.001f));
	REQUIRE(shader->runScalar({4.0f}) == Catch::Approx(0.25f).margin(0.001f));
	REQUIRE(shader->runScalar({2.0f}) == Catch::Approx(0.5f).margin(0.001f));
	REQUIRE(shader->runScalar({1.0f}) == Catch::Approx(1.0f).margin(0.001f));
	REQUIRE(shader->runScalar({0.5f}) == Catch::Approx(2.0f).margin(0.001f));
	REQUIRE(shader->runScalar({0.25f}) == Catch::Approx(4.0f).margin(0.001f));
	REQUIRE(shader->runScalar({0.125f}) == Catch::Approx(8.0f).margin(0.002f));
	REQUIRE(shader->runScalar({0.0625f}) == Catch::Approx(16.0f).margin(0.004f));
}

SHADER_TEST_CASE("RSQ", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::RSQ, output0, input0},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({-0.0f}) == INFINITY);
	REQUIRE(shader->runScalar({INFINITY}) == 0.0f);
	REQUIRE(std::isnan(shader->runScalar({-2.0f})));
	REQUIRE(std::isnan(shader->runScalar({-INFINITY})));
	REQUIRE(std::isnan(shader->runScalar({NAN})));
	REQUIRE(shader->runScalar({16.0f}) == Catch::Approx(0.25f).margin(0.001f));
	REQUIRE(shader->runScalar({8.0f}) == Catch::Approx(1.0f / std::sqrt(8.0f)).margin(0.001f));
	REQUIRE(shader->runScalar({4.0f}) == Catch::Approx(0.5f).margin(0.001f));
	REQUIRE(shader->runScalar({2.0f}) == Catch::Approx(1.0f / std::sqrt(2.0f)).margin(0.001f));
	REQUIRE(shader->runScalar({1.0f}) == Catch::Approx(1.0f).margin(0.001f));
	REQUIRE(shader->runScalar({0.5f}) == Catch::Approx(1.0f / std::sqrt(0.5f)).margin(0.001f));
	REQUIRE(shader->runScalar({0.25f}) == Catch::Approx(2.0f).margin(0.001f));
	REQUIRE(shader->runScalar({0.125f}) == Catch::Approx(1.0 / std::sqrt(0.125)).margin(0.002f));
	REQUIRE(shader->runScalar({0.0625f}) == Catch::Approx(4.0f).margin(0.004f));
}

SHADER_TEST_CASE("LG2", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::LG2, output0, input0},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(std::isnan(shader->runScalar({NAN})));
	REQUIRE(std::isnan(shader->runScalar({-1.f})));
	REQUIRE(std::isinf(shader->runScalar({0.f})));
	REQUIRE(shader->runScalar({4.f}) == Catch::Approx(2.f));
	REQUIRE(shader->runScalar({64.f}) == Catch::Approx(6.f));
	REQUIRE(shader->runScalar({1.e24f}) == Catch::Approx(79.7262742773f));
}

SHADER_TEST_CASE("EX2", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::EX2, output0, input0},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(std::isnan(shader->runScalar({NAN})));
	REQUIRE(shader->runScalar({-800.f}) == Catch::Approx(0.f));
	REQUIRE(shader->runScalar({0.f}) == Catch::Approx(1.f));
	REQUIRE(shader->runScalar({2.f}) == Catch::Approx(4.f));
	REQUIRE(shader->runScalar({6.f}) == Catch::Approx(64.f));
	REQUIRE(shader->runScalar({79.7262742773f}) == Catch::Approx(1.e24f));
	REQUIRE(std::isinf(shader->runScalar({800.f})));
}

SHADER_TEST_CASE("MAX", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::MAX, output0, input0, input1},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({1.0f, 0.0f}) == 1.0f);
	REQUIRE(shader->runScalar({0.0f, 1.0f}) == 1.0f);
	REQUIRE(shader->runScalar({0.0f, +INFINITY}) == +INFINITY);
	REQUIRE(shader->runScalar({0.0f, -INFINITY}) == -INFINITY);
	REQUIRE(shader->runScalar({NAN, 0.0f}) == 0.0f);
	REQUIRE(shader->runScalar({-INFINITY, +INFINITY}) == +INFINITY);
	REQUIRE(std::isnan(shader->runScalar({0.0f, NAN})));
}

SHADER_TEST_CASE("MIN", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::MIN, output0, input0, input1},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({1.0f, 0.0f}) == 0.0f);
	REQUIRE(shader->runScalar({0.0f, 1.0f}) == 0.0f);
	REQUIRE(shader->runScalar({0.0f, +INFINITY}) == 0.0f);
	REQUIRE(shader->runScalar({0.0f, -INFINITY}) == -INFINITY);
	REQUIRE(shader->runScalar({NAN, 0.0f}) == 0.0f);
	REQUIRE(shader->runScalar({-INFINITY, +INFINITY}) == -INFINITY);
	REQUIRE(std::isnan(shader->runScalar({0.0f, NAN})));
}

SHADER_TEST_CASE("SGE", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::SGE, output0, input0, input1},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({INFINITY, 0.0f}) == 1.0f);
	REQUIRE(shader->runScalar({0.0f, INFINITY}) == 0.0f);
	REQUIRE(shader->runScalar({NAN, 0.0f}) == 0.0f);
	REQUIRE(shader->runScalar({0.0f, NAN}) == 0.0f);
	REQUIRE(shader->runScalar({+INFINITY, +INFINITY}) == 1.0f);
	REQUIRE(shader->runScalar({+INFINITY, -INFINITY}) == 1.0f);
	REQUIRE(shader->runScalar({-INFINITY, +INFINITY}) == 0.0f);
	REQUIRE(shader->runScalar({+1.0f, -1.0f}) == 1.0f);
	REQUIRE(shader->runScalar({-1.0f, +1.0f}) == 0.0f);
}

SHADER_TEST_CASE("SLT", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::SLT, output0, input0, input1},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({INFINITY, 0.0f}) == 0.0f);
	REQUIRE(shader->runScalar({0.0f, INFINITY}) == 1.0f);
	REQUIRE(shader->runScalar({NAN, 0.0f}) == 0.0f);
	REQUIRE(shader->runScalar({0.0f, NAN}) == 0.0f);
	REQUIRE(shader->runScalar({+INFINITY, +INFINITY}) == 0.0f);
	REQUIRE(shader->runScalar({+INFINITY, -INFINITY}) == 0.0f);
	REQUIRE(shader->runScalar({-INFINITY, +INFINITY}) == 1.0f);
	REQUIRE(shader->runScalar({+1.0f, -1.0f}) == 0.0f);
	REQUIRE(shader->runScalar({-1.0f, +1.0f}) == 1.0f);
}

SHADER_TEST_CASE("FLR", "[shader][vertex]") {
	const auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::FLR, output0, input0},
		{nihstro::OpCode::Id::END},
	});

	REQUIRE(shader->runScalar({0.5}) == 0.0f);
	REQUIRE(shader->runScalar({-0.5}) == -1.0f);
	REQUIRE(shader->runScalar({1.5}) == 1.0f);
	REQUIRE(shader->runScalar({-1.5}) == -2.0f);
	REQUIRE(std::isnan(shader->runScalar({NAN})));
	REQUIRE(std::isinf(shader->runScalar({INFINITY})));
}