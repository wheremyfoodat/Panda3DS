#include <nihstro/inline_assembly.h>

#include <PICA/dynapica/shader_rec.hpp>
#include <PICA/shader.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <memory>
#include <span>

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

class ShaderInterpreterTest {
  protected:
	std::unique_ptr<PICAShader> shader = {};

	virtual void runShader() { shader->run(); }

  public:
	explicit ShaderInterpreterTest(std::initializer_list<nihstro::InlineAsm> code) : shader(assembleVertexShader(code)) {}

	std::span<const std::array<Floats::f24, 4>> runTest(std::span<const std::array<Floats::f24, 4>> inputs) {
		std::copy(inputs.begin(), inputs.end(), shader->inputs.begin());
		runShader();
		return shader->outputs;
	}

	// Each input is written to the x component of sequential input registers
	// The first output vector is returned
	const std::array<Floats::f24, 4>& runVector(std::initializer_list<float> inputs) {
		std::vector<std::array<Floats::f24, 4>> inputsVec;
		for (const float& input : inputs) {
			const std::array<Floats::f24, 4> inputVec = {
				f24::fromFloat32(input),
				f24::zero(),
				f24::zero(),
				f24::zero(),
			};
			inputsVec.emplace_back(inputVec);
		}
		return runTest(inputsVec)[0];
	}

	// Each input is written to the x component of sequential input registers
	// The x component of the first output
	float runScalar(std::initializer_list<float> inputs) { return runVector(inputs)[0].toFloat32(); }

	[[nodiscard]] std::array<std::array<Floats::f24, 4>, 96>& floatUniforms() const { return shader->floatUniforms; }
	[[nodiscard]] std::array<std::array<u8, 4>, 4>& intUniforms() const { return shader->intUniforms; }
	[[nodiscard]] u32& boolUniform() const { return shader->boolUniform; }

	static std::unique_ptr<ShaderInterpreterTest> assembleTest(std::initializer_list<nihstro::InlineAsm> code) {
		return std::make_unique<ShaderInterpreterTest>(code);
	}
};

#if defined(PANDA3DS_SHADER_JIT_SUPPORTED)
class ShaderJITTest final : public ShaderInterpreterTest {
  private:
	ShaderJIT shaderJit = {};

	void runShader() override { shaderJit.run(*shader); }

  public:
	explicit ShaderJITTest(std::initializer_list<nihstro::InlineAsm> code) : ShaderInterpreterTest(code) { shaderJit.prepare(*shader); }

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

SHADER_TEST_CASE("Uniform Read", "[shader][vertex][uniform]") {
	const auto constant0 = nihstro::SourceRegister::MakeFloat(0);
	auto shader = TestType::assembleTest({
		{nihstro::OpCode::Id::MOVA, nihstro::DestRegister{}, "x", input0, "x", nihstro::SourceRegister{}, "", nihstro::InlineAsm::RelativeAddress::A1
		},
		{nihstro::OpCode::Id::MOV, output0, "xyzw", constant0, "xyzw", nihstro::SourceRegister{}, "", nihstro::InlineAsm::RelativeAddress::A1},
		{nihstro::OpCode::Id::END},
	});

	// Generate float uniforms
	std::array<std::array<Floats::f24, 4>, 96> floatUniforms = {};
	for (u32 i = 0; i < 96; ++i) {
		const float color = (i * 2.0f) / 255.0f;
		const Floats::f24 color24 = Floats::f24::fromFloat32(color);
		const std::array<Floats::f24, 4> testValue = {color24, color24, color24, Floats::f24::fromFloat32(1.0f)};
		shader->floatUniforms()[i] = testValue;
		floatUniforms[i] = testValue;
	}

	for (u32 i = 0; i < 96; ++i) {
		const float index = static_cast<float>(i);
		// Intentionally use some fractional values to verify float->integer
		// truncation during address translation
		const float fractional = (i % 17) / 17.0f;

		REQUIRE(shader->runVector({index + fractional}) == floatUniforms[i]);
	}
}