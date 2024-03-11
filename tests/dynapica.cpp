#include <nihstro/inline_assembly.h>

#include <PICA/dynapica/shader_rec.hpp>
#include <PICA/shader.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <memory>

using namespace Floats;

#if defined(PANDA3DS_SHADER_JIT_SUPPORTED)

static std::unique_ptr<PICAShader> assembleVertexShader(std::initializer_list<nihstro::InlineAsm> code) {
	const auto shbin = nihstro::InlineAsm::CompileToRawBinary(code);

	auto shader = std::make_unique<PICAShader>(ShaderType::Vertex);
	shader->reset();

	for (const nihstro::Instruction& instruction : shbin.program) {
		shader->uploadWord(instruction.hex);
	}
	for (const nihstro::SwizzlePattern& swizzle : shbin.swizzle_table) {
		shader->uploadDescriptor(swizzle.hex);
	}
	shader->finalize();
	return shader;
}

class VertexShaderTest {
  private:
	std::unique_ptr<PICAShader> shader;

  public:
	explicit VertexShaderTest(std::initializer_list<nihstro::InlineAsm> code) : shader(assembleVertexShader(code)) {}

	// Multiple inputs, singular scalar output
	float RunScalar(std::initializer_list<float> inputs) {
		usize input_index = 0;
		for (const float& input : inputs) {
			const std::array<Floats::f24, 4> input_vec = std::array<Floats::f24, 4>{f24::fromFloat32(1.0), f24::zero(), f24::zero(), f24::zero()};
			shader->inputs[input_index++] = input_vec;
		}
		shader->run();
		return shader->outputs[0][0];
	}

	static std::unique_ptr<VertexShaderTest> assembleTest(std::initializer_list<nihstro::InlineAsm> code) {
		return std::make_unique<VertexShaderTest>(code);
	}
};

TEST_CASE("ADD", "[shader][vertex]") {
	const nihstro::SourceRegister input0 = nihstro::SourceRegister::MakeInput(0);
	const nihstro::SourceRegister input1 = nihstro::SourceRegister::MakeInput(1);
	const nihstro::DestRegister output0 = nihstro::DestRegister::MakeOutput(0);

	const auto shader = VertexShaderTest::assembleTest({
		{nihstro::OpCode::Id::ADD, output0, input0, input1},
		{nihstro::OpCode::Id::END},
	});
	REQUIRE(shader->RunScalar({+1.0f, -1.0f}) == +0.0f);
	REQUIRE(shader->RunScalar({+0.0f, -0.0f}) == -0.0f);
	REQUIRE(std::isnan(shader->RunScalar({+INFINITY, -INFINITY})));
	REQUIRE(std::isinf(shader->RunScalar({INFINITY, +1.0f})));
	REQUIRE(std::isinf(shader->RunScalar({INFINITY, -1.0f})));
}

#endif