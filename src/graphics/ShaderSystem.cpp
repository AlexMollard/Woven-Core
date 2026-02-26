#include "pch.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <slang-com-ptr.h>
#include <slang.h>

#include "core/Logger.hpp"
#include "graphics/ShaderSystem.hpp"

namespace
{
	constexpr uint32_t kSpirvMagic = 0x07230203u;

	std::string ToHex(uint32_t value)
	{
		char buffer[11] = {};
		std::snprintf(buffer, sizeof(buffer), "0x%08X", value);
		return std::string(buffer);
	}

	void DumpSpirvToFile(const std::filesystem::path& path, const std::vector<uint32_t>& spirv)
	{
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		std::ofstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			return;
		}
		file.write(reinterpret_cast<const char*>(spirv.data()), static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
	}

} // namespace

struct ShaderSystem::SlangHandles
{
	Slang::ComPtr<slang::IGlobalSession> globalSession;
	Slang::ComPtr<slang::ISession> session;
	SlangProfileID profile = SLANG_PROFILE_UNKNOWN;
};

ShaderSystem::ShaderSystem() = default;

ShaderSystem::~ShaderSystem()
{
	Shutdown();
}

bool ShaderSystem::Initialize(VkDevice device, VkDescriptorSetLayout bindlessLayout, const VkPushConstantRange& pushConstants)
{
	m_Device = device;
	m_BindlessLayout = bindlessLayout;
	m_PushConstantRange = pushConstants;

	m_Slang = std::make_unique<SlangHandles>();
	SlangGlobalSessionDesc globalDesc{};
	globalDesc.apiVersion = SLANG_API_VERSION;
	if (SLANG_FAILED(slang::createGlobalSession(&globalDesc, m_Slang->globalSession.writeRef())))
	{
		Logger::Error("Failed to create Slang global session");
		return false;
	}

	const SlangProfileID hlslProfile = m_Slang->globalSession->findProfile("sm_6_6");
	if (hlslProfile == SLANG_PROFILE_UNKNOWN)
	{
		Logger::Error("Failed to find Slang profile sm_6_6");
		return false;
	}

	m_Slang->profile = m_Slang->globalSession->findProfile("spirv_1_5");
	if (m_Slang->profile == SLANG_PROFILE_UNKNOWN)
	{
		Logger::Error("Failed to find Slang profile spirv_1_5");
		return false;
	}

	std::filesystem::path shaderDir = std::filesystem::current_path() / "shaders";

	if (!std::filesystem::exists(shaderDir))
	{
		std::filesystem::path probe = shaderDir.parent_path();
		for (int i = 0; i < 5 && !probe.empty(); ++i)
		{
			std::filesystem::path candidate = probe / "shaders";
			if (std::filesystem::exists(candidate))
			{
				shaderDir = candidate;
				break;
			}
			probe = probe.parent_path();
		}
	}

	if (!std::filesystem::exists(shaderDir))
	{
		Logger::Warning("Shader directory not found: %s", shaderDir.string().c_str());
	}

	m_SearchPaths.clear();
	m_SearchPaths.push_back(shaderDir.string());

	std::vector<const char*> searchPathCStr;
	searchPathCStr.reserve(m_SearchPaths.size());
	for (const auto& path: m_SearchPaths)
	{
		searchPathCStr.push_back(path.c_str());
	}

	slang::TargetDesc targetDesc{};
	targetDesc.format = SLANG_SPIRV;
	targetDesc.profile = m_Slang->profile;
	targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

	slang::SessionDesc sessionDesc{};
	sessionDesc.targets = &targetDesc;
	sessionDesc.targetCount = 1;
	sessionDesc.searchPaths = searchPathCStr.data();
	sessionDesc.searchPathCount = static_cast<SlangInt>(searchPathCStr.size());
	sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

	if (SLANG_FAILED(m_Slang->globalSession->createSession(sessionDesc, m_Slang->session.writeRef())))
	{
		Logger::Error("Failed to create Slang session");
		return false;
	}

	Logger::Info("Slang initialized (shader dir: %s, target: spirv_1_5)", shaderDir.string().c_str());
	return true;
}

void ShaderSystem::Shutdown()
{
	m_Slang.reset();
	m_Device = VK_NULL_HANDLE;
	m_BindlessLayout = VK_NULL_HANDLE;
}

bool ShaderSystem::CreateShaderObject(const ShaderCompileDesc& desc, VkShaderEXT& outShader)
{
	outShader = VK_NULL_HANDLE;
	std::vector<uint32_t> spirv;
	if (!CompileToSpirv(desc, spirv))
	{
		return false;
	}

	const char* spirvEntryPoint = "main";
	VkShaderCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
	createInfo.stage = desc.stage;
	createInfo.nextStage = 0;
	createInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
	createInfo.codeSize = spirv.size() * sizeof(uint32_t);
	createInfo.pCode = spirv.data();
	createInfo.pName = spirvEntryPoint;
	createInfo.setLayoutCount = 1;
	createInfo.pSetLayouts = &m_BindlessLayout;
	createInfo.pushConstantRangeCount = 1;
	createInfo.pPushConstantRanges = &m_PushConstantRange;

	if (vkCreateShadersEXT(m_Device, 1, &createInfo, nullptr, &outShader) != VK_SUCCESS)
	{
		Logger::Error("Failed to create shader object: %s", desc.filePath.c_str());
		return false;
	}

	Logger::Info("Shader object created: %s (%s -> %s)", desc.filePath.c_str(), desc.entryPoint.c_str(), spirvEntryPoint);
	return true;
}

void ShaderSystem::DestroyShader(VkShaderEXT shader)
{
	if (shader != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
	{
		vkDestroyShaderEXT(m_Device, shader, nullptr);
	}
}

bool ShaderSystem::CompileToSpirv(const ShaderCompileDesc& desc, std::vector<uint32_t>& outSpirv)
{
	if (!m_Slang || !m_Slang->session)
	{
		Logger::Error("Slang session not initialized");
		return false;
	}

	const std::string moduleName = GetModuleName(desc.filePath);
	if (moduleName.empty())
	{
		Logger::Error("Invalid shader file path: %s", desc.filePath.c_str());
		return false;
	}

	Slang::ComPtr<slang::IBlob> diagnostics;
	Slang::ComPtr<slang::IModule> module(m_Slang->session->loadModule(moduleName.c_str(), diagnostics.writeRef()));
	if (!module)
	{
		Logger::Error("Slang failed to load module %s: %s", moduleName.c_str(), GetDiagnosticsString(diagnostics.get()).c_str());
		return false;
	}
	if (diagnostics && diagnostics->getBufferSize() > 0)
	{
		Logger::Warning("Slang module diagnostics (%s): %s", moduleName.c_str(), GetDiagnosticsString(diagnostics.get()).c_str());
	}

	Slang::ComPtr<slang::IEntryPoint> entryPoint;
	if (SLANG_FAILED(module->findEntryPointByName(desc.entryPoint.c_str(), entryPoint.writeRef())))
	{
		Logger::Error("Slang failed to find entry point %s in %s", desc.entryPoint.c_str(), moduleName.c_str());
		return false;
	}

	Slang::ComPtr<slang::IComponentType> composedProgram;
	slang::IComponentType* components[] = { module.get(), entryPoint.get() };
	if (SLANG_FAILED(m_Slang->session->createCompositeComponentType(components, 2, composedProgram.writeRef())))
	{
		Logger::Error("Slang failed to compose program for %s:%s", moduleName.c_str(), desc.entryPoint.c_str());
		return false;
	}

	Slang::ComPtr<slang::IComponentType> linkedProgram;
	Slang::ComPtr<slang::IBlob> linkDiagnostics;
	if (SLANG_FAILED(composedProgram->link(linkedProgram.writeRef(), linkDiagnostics.writeRef())))
	{
		Logger::Error("Slang link failed for %s:%s: %s", moduleName.c_str(), desc.entryPoint.c_str(), GetDiagnosticsString(linkDiagnostics.get()).c_str());
		return false;
	}
	if (linkDiagnostics && linkDiagnostics->getBufferSize() > 0)
	{
		Logger::Warning("Slang link diagnostics (%s:%s): %s", moduleName.c_str(), desc.entryPoint.c_str(), GetDiagnosticsString(linkDiagnostics.get()).c_str());
	}

	Slang::ComPtr<slang::IBlob> spirvBlob;
	Slang::ComPtr<slang::IBlob> spirvDiagnostics;
	if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), spirvDiagnostics.writeRef())))
	{
		Logger::Error("Slang SPIR-V emission failed for %s:%s: %s", moduleName.c_str(), desc.entryPoint.c_str(), GetDiagnosticsString(spirvDiagnostics.get()).c_str());
		return false;
	}
	if (spirvDiagnostics && spirvDiagnostics->getBufferSize() > 0)
	{
		Logger::Warning("Slang SPIR-V diagnostics (%s:%s): %s", moduleName.c_str(), desc.entryPoint.c_str(), GetDiagnosticsString(spirvDiagnostics.get()).c_str());
	}

	const size_t byteSize = spirvBlob->getBufferSize();
	if (byteSize == 0)
	{
		Logger::Error("Slang produced empty SPIR-V for %s:%s", moduleName.c_str(), desc.entryPoint.c_str());
		return false;
	}
	if (byteSize % sizeof(uint32_t) != 0)
	{
		Logger::Warning("SPIR-V byte size is not 4-byte aligned for %s:%s", moduleName.c_str(), desc.entryPoint.c_str());
	}

	outSpirv.resize((byteSize + sizeof(uint32_t) - 1) / sizeof(uint32_t));
	std::memcpy(outSpirv.data(), spirvBlob->getBufferPointer(), byteSize);

	if (outSpirv.size() < 5)
	{
		Logger::Error("SPIR-V too small for %s:%s (words: %zu)", moduleName.c_str(), desc.entryPoint.c_str(), outSpirv.size());
		return false;
	}

	if (outSpirv[0] != kSpirvMagic)
	{
		Logger::Error("Invalid SPIR-V magic for %s:%s (magic: %s)", moduleName.c_str(), desc.entryPoint.c_str(), ToHex(outSpirv[0]).c_str());
	}

	Logger::Info("SPIR-V header %s:%s (magic %s, version %s, words %zu)", moduleName.c_str(), desc.entryPoint.c_str(), ToHex(outSpirv[0]).c_str(), ToHex(outSpirv[1]).c_str(), outSpirv.size());

	const std::filesystem::path cacheDir = std::filesystem::current_path() / "shader_cache";
	const std::filesystem::path dumpPath = cacheDir / (moduleName + "_" + desc.entryPoint + ".spv");
	DumpSpirvToFile(dumpPath, outSpirv);
	Logger::Info("Wrote SPIR-V to %s", dumpPath.string().c_str());
	return true;
}

std::string ShaderSystem::GetModuleName(const std::string& filePath) const
{
	std::filesystem::path path(filePath);
	return path.stem().string();
}

std::string ShaderSystem::GetDiagnosticsString(void* diagnosticsBlob) const
{
	auto* blob = reinterpret_cast<slang::IBlob*>(diagnosticsBlob);
	if (!blob)
	{
		return {};
	}
	return std::string(static_cast<const char*>(blob->getBufferPointer()), blob->getBufferSize());
}
